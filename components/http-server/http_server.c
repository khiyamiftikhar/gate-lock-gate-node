#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include <string.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include "cJSON.h"
#include "esp_random.h"     // esp_random()
#include "http_server.h"
#include "event_system_adapter.h"


#define         MAX_URIS                    5
#define         MAX_URI_LENGTH              15
#define         USERNAME                    "admin"
#define         PASSWORD                    "pindora"

#define MAX_USERS       1  // Can scale later
#define TOTAL_BUFFERS   2  // Can scale later

#define MIN(a,b) ((a) < (b) ? (a) : (b))

extern const uint8_t login_html_start[]     asm("_binary_login_html_start");
extern const uint8_t login_html_end[]       asm("_binary_login_html_end");
extern const uint8_t dashboard_html_start[] asm("_binary_dashboard_html_start");
extern const uint8_t dashboard_html_end[]   asm("_binary_dashboard_html_end");

extern const uint8_t ota_html_start[]       asm("_binary_ota_html_start");
extern const uint8_t ota_html_end[]         asm("_binary_ota_html_end");

extern const uint8_t login_js_start[]     asm("_binary_login_js_start");
extern const uint8_t login_js_end[]       asm("_binary_login_js_end");
extern const uint8_t dashboard_js_start[] asm("_binary_dashboard_js_start");
extern const uint8_t dashboard_js_end[]   asm("_binary_dashboard_js_end");

extern const uint8_t ota_js_start[]       asm("_binary_ota_js_start");
extern const uint8_t ota_js_end[]         asm("_binary_ota_js_end");




typedef struct {
    bool active;
    char username[32];
    char token[64];
    uint32_t last_activity;
} session_t;

static session_t current_sessions[MAX_USERS] = {0};


//Multiple buffers/buckets in the pool
//Sent to ota, and ota returns after usage
//Meanwhile the file upload uses  other from the pool if available
#define TOTAL_CHUNK_BUFFERS      3      
#define UPLOAD_CHUNK_SIZE       1024    

static struct{
    httpd_handle_t server_handle;
    char recv_buf[TOTAL_CHUNK_BUFFERS][UPLOAD_CHUNK_SIZE];   
    QueueHandle_t buffer_queue;
   // bool logged_in;
    
}http_server={0};
  


static const char *TAG = "HTTP Server";



#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)






DEFINE_EVENT_ADAPTER(HTTP_SERVER);


/// @brief Get a buffer from pool
/// @param wait_time in ms
/// @return 
static char* get_chunk_buffer_from_pool(uint32_t wait_time){

    char* recv_buf;
    //Specifying the address of pointer, because an address is to be copied
    if(xQueueReceive(http_server.buffer_queue,&recv_buf,pdMS_TO_TICKS(wait_time))!=pdTRUE)
        return NULL;

    ESP_LOGI(TAG,"buff address %p",recv_buf);
    return recv_buf;
}

esp_err_t http_server_return_chunk_buffer(char* buffer){
    
    
    //Supplies address  of pointer to get data, so copies address of mem it points
    ESP_LOGI(TAG,"ret address %p",buffer);
    if(xQueueSend(http_server.buffer_queue,&buffer,portMAX_DELAY)!=pdTRUE)
        return ESP_FAIL;

    return ESP_OK;
}

static bool is_user_logged_in(void) {
    for (int i = 0; i < MAX_USERS; ++i) {
        if (current_sessions[i].active)
            return true;
    }
    return false;
}

static esp_err_t redirect_to_login(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/login.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


static bool validate_token(const char *token)
{
    if (!token || strlen(token) == 0)
        return false;

    //time_t now = time(NULL);

    for (int i = 0; i < MAX_USERS; ++i) {

        ESP_LOGI(TAG, "comparison %d",strcmp(current_sessions[i].token, token));
        ESP_LOGI(TAG,"saved token %s", current_sessions[i].token);
        if (current_sessions[i].active &&
            strcmp(current_sessions[i].token, token) == 0) {
            ESP_LOGI(TAG,"sess active");
            uint32_t current_ticks=xTaskGetTickCount()*portTICK_PERIOD_MS;
            if ((current_ticks - current_sessions[i].last_activity) > 180000) {
                current_sessions[i].active = false;
                return false;
            }

            // update last activity
            current_sessions[i].last_activity = current_ticks;
            ESP_LOGI(TAG,"match found");
            return true;
        }
    }
    return false;
}








#define TOKEN_LEN 32


/* Generate a random token (alphanumeric). out must have space for len bytes (including NUL). */
static void generate_token(char *out, size_t len)
{
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const size_t charset_len = sizeof(charset) - 1; // exclude terminating NUL

    if (len == 0) return;
    for (size_t i = 0; i < len - 1; ++i) {
        uint32_t r = esp_random();          // 32-bit RNG
        out[i] = charset[r % charset_len];
    }
    out[len - 1] = '\0';
}


#define LOGIN_BUF_SIZE 256

static esp_err_t auth_handler(httpd_req_t *req)
{
    char buf[LOGIN_BUF_SIZE + 1];
    int ret = httpd_req_recv(req, buf, LOGIN_BUF_SIZE);
    if (ret <= 0) {
        ESP_LOGW(TAG, "No data received or error: %d", ret);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse JSON bod y
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse error");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *juser = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON *jpass = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(juser) || !cJSON_IsString(jpass)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "username/password required");
        return ESP_FAIL;
    }

    const char *username = juser->valuestring;
    const char *password = jpass->valuestring;

    // TODO: Replace these with your real credential check
    const char *EXPECTED_USER = USERNAME;
    const char *EXPECTED_PASS = PASSWORD;

    if (strcmp(username, EXPECTED_USER) == 0 && strcmp(password, EXPECTED_PASS) == 0) {
        // Auth OK -> generate token
        char token[TOKEN_LEN + 1];
        generate_token(token, sizeof(token));

    for (int i = 0; i < MAX_USERS; ++i) {

        if (!current_sessions[i].active) {
            ESP_LOGI(TAG,"saving token");
            current_sessions[i].active = true;
            //strncpy(current_sessions[i].username, username, sizeof(current_sessions[i].username));
            strncpy(current_sessions[i].token, token, sizeof(current_sessions[i].token));
            current_sessions[i].last_activity=xTaskGetTickCount()*portTICK_PERIOD_MS; //Time in milliseconds
            break;
        }
    }

        // NOTE: currently we are NOT saving token on server.
        // Later: store token in sessions[] to support logout/expiry/validation.

        // Build JSON response
        cJSON *resp = cJSON_CreateObject();
        if (!resp) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory error");
            return ESP_FAIL;
        }
        cJSON_AddStringToObject(resp, "result", "ok");
        cJSON_AddStringToObject(resp, "token", token);

        char *out = cJSON_PrintUnformatted(resp);
        if (!out) {
            cJSON_Delete(resp);
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory error");
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, out);

        free(out);
        cJSON_Delete(resp);
        cJSON_Delete(root);
        //current_sessions[0].active=true;
        ESP_LOGI(TAG, "User '%s' logged in, issued token (len %d)", username, (int)strlen(token));
        return ESP_OK;
    } else {
        // Auth failed
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "result", "fail");
        cJSON_AddStringToObject(resp, "message", "Invalid credentials");
        char *out = cJSON_PrintUnformatted(resp);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, out ? out : "{\"result\":\"fail\"}");

        if (out) free(out);
        cJSON_Delete(resp);
        cJSON_Delete(root);

        ESP_LOGW(TAG, "Authentication failed for user '%s'", username);
        return ESP_OK;
    }
}


static esp_err_t logout_handler(httpd_req_t *req)
{
    char token[64];
    if (httpd_req_get_hdr_value_str(req, "Authorization", token, sizeof(token)) == ESP_OK) {
        ESP_LOGI(TAG,"main if %s",token);
        char *p = strstr(token, "Bearer ");
        ESP_LOGI(TAG,"token %s",token);
            //validate_token(p + 7);
        if (p && validate_token(p + 7)){
            ESP_LOGI(TAG,"token matched");
            current_sessions[0].active = false;
            memset(current_sessions, 0, sizeof(current_sessions));
            httpd_resp_sendstr(req, "{\"result\":\"logged_out\"}");
            return ESP_OK;
        }
    }

    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid token");
    return ESP_FAIL;
}


static esp_err_t logged_in_checked(httpd_req_t *req){
    //Logged in check
    char auth_header[128] = {0};

    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK) {
        ESP_LOGI(TAG,"redirect Auth h");
        return ESP_FAIL;//redirect_to_login(req);
    }
    ESP_LOGI(TAG, "Auth header: '%s'", auth_header);
    // Expect header "Authorization: Bearer <token>"
    char *p = strstr(auth_header, "Bearer ");
    if (!p || !validate_token(p + 7)) {
        ESP_LOGI(TAG,"redirect Bearer h");
        return ESP_FAIL;//redirect_to_login(req);
    }

    return ESP_OK;
}


static esp_err_t check_auth_handler(httpd_req_t *req)
{
    // Reuse your existing check
    if (logged_in_checked(req) != ESP_OK) {
        // Return 401 Unauthorized (not redirect HTML, since it's JS calling)
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // If OK → send simple JSON response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}


static esp_err_t file_upload_handler(httpd_req_t *req)
{
        
    
    
    //ESP_LOGI(TAG,"logged user");
    int received;

    char* recv_buf;
    
    
    
    //Post that file transfer is started, so that ota perfomrs thee inits required
    //It is written outside assuming the first buffer  will always be retereived so no risk
    //Because otherwise this signal must be sent if buffer is  acquired
    HTTP_SERVER_post_event(HTTP_SERVER_EVENT_FILE_TRANSFER_STARTED,NULL,0);         
    
    ESP_LOGI(TAG, "File upload started, content length: %d", req->content_len);
    size_t total_received = 0;
    while(1){

        char* recv_buf=NULL;
        recv_buf=get_chunk_buffer_from_pool(10);

        if(recv_buf==NULL){
            ESP_LOGE(TAG, "Mem not available");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Mem not available");
            return ESP_FAIL;        
        }
        
        received = httpd_req_recv(req, recv_buf, UPLOAD_CHUNK_SIZE);

        if (received < 0) {
        ESP_LOGE(TAG, "File receive error: %d", received);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
        HTTP_SERVER_post_event(HTTP_SERVER_EVENT_FILE_TRASFER_FAILED,NULL,0);         
        return ESP_FAIL;
        }

        if (received == 0) {
            ESP_LOGE(TAG, "File receive complete");
            //httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            HTTP_SERVER_post_event(HTTP_SERVER_EVENT_FILE_TRANSFER_COMPLETE,NULL,0);         
            break;
         }
        total_received += received;

        HTTP_SERVER_post_event(HTTP_SERVER_EVENT_FILE_CHUNK_ARRIVED,(const void*)recv_buf,received);
            

        ESP_LOGD(TAG, "Received %d bytes (total %u)", received, total_received);
    }

    

    ESP_LOGI(TAG, "File upload completed, total received: %u bytes", total_received);

    // When done reading, notify completion (you’ll post this too later)
    // esp_event_post_to(ota_event_loop, OTA_EVENT_COMPLETE, NULL, 0, portMAX_DELAY);

    httpd_resp_sendstr(req, "Upload successful");

    //Just for testing . this mustb be called by ota
    
    return ESP_OK;
}




//IT is using chunk send
static esp_err_t http_server_send_html_resource(httpd_req_t *req,
                                                const char *page_data,
                                                size_t len)
{
 if (!req || !page_data) {
        return ESP_ERR_INVALID_ARG;
    }


    
    esp_err_t ret;

    ret = httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (ret != ESP_OK) return ret;

    ret = httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    if (ret != ESP_OK) return ret;

    // Enable chunked encoding
    //ret = httpd_resp_send_chunk(req, NULL, 0); // Start the response
    //if (ret != ESP_OK) return ret;

    const size_t chunk_size = 1024; // 1 KB chunks — safe size
    for (size_t offset = 0; offset < len; offset += chunk_size) {
        size_t this_chunk = MIN(chunk_size, len - offset);
        ret = httpd_resp_send_chunk(req, page_data + offset, this_chunk);
        if (ret != ESP_OK) {
            httpd_resp_send_chunk(req, NULL, 0); // End response safely
            return ret;
        }

        // yield to avoid starving other tasks
        vTaskDelay(1);
    }

    // Indicate end of response
    return httpd_resp_send_chunk(req, NULL, 0);
}    

//IT is using chunk send
static esp_err_t http_server_send_js_resource(httpd_req_t *req,
                                                const char *page_data,
                                                size_t len)
{
 if (!req || !page_data) {
        return ESP_ERR_INVALID_ARG;
    }


    
    esp_err_t ret;

    ret = httpd_resp_set_type(req, "application/javascript");
    if (ret != ESP_OK) return ret;

    ret = httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    if (ret != ESP_OK) return ret;

    // Enable chunked encoding
    //ret = httpd_resp_send_chunk(req, NULL, 0); // Start the response
    //if (ret != ESP_OK) return ret;

    const size_t chunk_size = 1024; // 1 KB chunks — safe size
    for (size_t offset = 0; offset < len; offset += chunk_size) {
        size_t this_chunk = MIN(chunk_size, len - offset);
        ret = httpd_resp_send_chunk(req, page_data + offset, this_chunk);
        if (ret != ESP_OK) {
            httpd_resp_send_chunk(req, NULL, 0); // End response safely
            return ret;
        }

        // yield to avoid starving other tasks
        vTaskDelay(1);
    }

    // Indicate end of response
    return httpd_resp_send_chunk(req, NULL, 0);
}    



static esp_err_t http_server_send_status_error(httpd_req_t* req,
                                       int status_code,
                                       const char* error_msg) {
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Convert status code to string
    char status_str[32];
    const char* status_text;
    
    switch (status_code) {
        case 400: status_text = "400 Bad Request"; break;
        case 401: status_text = "401 Unauthorized"; break;
        case 403: status_text = "403 Forbidden"; break;
        case 404: status_text = "404 Not Found"; break;
        case 405: status_text = "405 Method Not Allowed"; break;
        case 500: status_text = "500 Internal Server Error"; break;
        case 503: status_text = "503 Service Unavailable"; break;
        default:
            snprintf(status_str, sizeof(status_str), "%d Error", status_code);
            status_text = status_str;
            break;
    }
    
    esp_err_t ret = httpd_resp_set_status(req, status_text);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = httpd_resp_set_type(req, "text/plain");
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    if (ret != ESP_OK) {
        return ret;
    }
    
    const char* msg = error_msg ? error_msg : "An error occurred";
    return httpd_resp_send(req, msg, strlen(msg));
}


static esp_err_t login_html_handler(httpd_req_t* req){
        
    size_t len=login_html_end -login_html_start;
    ESP_LOGI(TAG,"login length %d",len);
    esp_err_t ret=0;
    ret=http_server_send_html_resource(req,(const char*)login_html_start,len);
    return ret;
}


static esp_err_t dashboard_html_handler(httpd_req_t* req){
        
    
    size_t len=dashboard_html_end - dashboard_html_start;

    esp_err_t ret=0;
    ret=http_server_send_html_resource(req,(const char*)dashboard_html_start,len);
    return ret;
}
static esp_err_t ota_html_handler(httpd_req_t* req){
        
    
    size_t len=ota_html_end - ota_html_start;



    esp_err_t ret=0;
    ret=http_server_send_html_resource(req,(const char*)ota_html_start,len);
    return ret;
}




static esp_err_t login_js_handler(httpd_req_t* req){
        
    size_t len=login_js_end -login_js_start;

    esp_err_t ret=0;
    ret=http_server_send_js_resource(req,(const char*)login_js_start,len);
    return ret;
}


static esp_err_t dashboard_js_handler(httpd_req_t* req){
        
    size_t len=dashboard_js_end - dashboard_js_start;

    esp_err_t ret=0;
    ret=http_server_send_js_resource(req,(const char*)dashboard_js_start,len);
    return ret;
}
static esp_err_t ota_js_handler(httpd_req_t* req){
        
    size_t len=ota_js_end - ota_js_start;

    esp_err_t ret=0;
    ret=http_server_send_js_resource(req,(const char*)ota_js_start,len);
    return ret;
}





esp_err_t http_server_init(http_server_config_t* config){

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    //http_config.max_open_sockets = config->max_connections;
    http_config.uri_match_fn = httpd_uri_match_wildcard;
    http_config.server_port = config->port;
    http_config.max_uri_handlers=13;
    ESP_LOGI(TAG, "Entered Server Init");
    http_server.buffer_queue=xQueueCreate(TOTAL_CHUNK_BUFFERS,sizeof(char*));

    if(http_server.buffer_queue==NULL)
        return ESP_FAIL;

    for(uint8_t i=0;i<TOTAL_CHUNK_BUFFERS;i++){
        
        char* p=http_server.recv_buf[i];
        ESP_LOGI(TAG,"buffer addresses %p",p);
        xQueueSend(http_server.buffer_queue,&p,portMAX_DELAY);
    }
    

    ESP_LOGI(TAG, "Starting HTTP Server");
    if( httpd_start(&http_server.server_handle, &http_config) != ESP_OK){
        ESP_LOGI(TAG,"Server Init Failed");
        return ESP_FAIL;
    }


    httpd_uri_t root_page = {
    .uri = "/",   // Root path
    .method = HTTP_GET,
    .handler = login_html_handler,  // same handler reused
    .user_ctx = NULL
    };

    httpd_register_uri_handler(http_server.server_handle, &root_page);



    //The first three URIs are for serving the html pages
    httpd_uri_t login_html_uri = {
        .uri      = "/login.html",
        .method   = HTTP_GET,
        .handler  = login_html_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &login_html_uri);




    httpd_uri_t dashboard_html_uri = {
        .uri      = "/dashboard.html",
        .method   = HTTP_GET,
        .handler  = dashboard_html_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &dashboard_html_uri);


    httpd_uri_t ota_html_uri = {
        .uri      = "/ota.html",
        .method   = HTTP_GET,
        .handler  = ota_html_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &ota_html_uri);

//The three get requests are made by browser when it parses the html and finds the use of these js
    httpd_uri_t login_js_uri = {
        .uri      = "/login.js",
        .method   = HTTP_GET,
        .handler  = login_js_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &login_js_uri);




    httpd_uri_t dashboard_js_uri = {
        .uri      = "/dashboard.js",
        .method   = HTTP_GET,
        .handler  = dashboard_js_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &dashboard_js_uri);


    httpd_uri_t ota_js_uri = {
        .uri      = "/ota.js",
        .method   = HTTP_GET,
        .handler  = ota_js_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &ota_js_uri);


    
    //These below are for actions such as login,logout and upload fimrware files

    httpd_uri_t login_uri = {
        .uri      = "/auth",
        .method   = HTTP_POST,
        .handler  = auth_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(http_server.server_handle, &login_uri);

    httpd_uri_t ota_upload_uri = {
        .uri      = "/ota/upload",
        .method   = HTTP_POST,
        .handler  = file_upload_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &ota_upload_uri);

    

    httpd_uri_t logout_uri = {
        .uri      = "/logout",
        .method   = HTTP_POST,
        .handler  = logout_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &logout_uri);


    httpd_uri_t checkauth_uri = {
        .uri      = "/check-auth",
        .method   = HTTP_GET,
        .handler  = check_auth_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server.server_handle, &checkauth_uri);



    //Tell the ota that file chunk will be transferred next
    HTTP_SERVER_register_event(HTTP_SERVER_EVENT_FILE_TRANSFER_STARTED,NULL);
    //When a file chunk arrives
    HTTP_SERVER_register_event(HTTP_SERVER_EVENT_FILE_CHUNK_ARRIVED,NULL);
    //When file recive fails
    HTTP_SERVER_register_event(HTTP_SERVER_EVENT_FILE_TRASFER_FAILED,NULL);
    //On successfull EOF (0 data recevied in last chunk)
    HTTP_SERVER_register_event(HTTP_SERVER_EVENT_FILE_TRANSFER_COMPLETE,NULL);
    return ESP_OK;

    
}



esp_err_t stopHttpServer(){
    return httpd_stop(http_server.server_handle);

}
