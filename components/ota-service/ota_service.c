/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "errno.h"
#include "ota_service.h"
#include "sync_manager.h"


#define MAX_HTTP_RECV_BUFFER    1024
#define HASH_LEN                32 /* SHA-256 digest length */
#define OTA_RECV_TIMEOUT        5000
#define MANIFEST_URL            CONFIG_FIRMWARE_URL
#define AUTO_CHECK_DURATION     CONFIG_AUTO_CHECK_DURATION

static const char *TAG = "ota_example";
/*an ota data write buffer ready to write to the flash*/

typedef enum{
    OTA_STATE_IDLE=0,     //At boot. When data arrival message comes it tries to get handle and then goes to active
                        //Only responds to the data arrival message. All other messages it ignores
                        //Because on any failure in active state, it comes back here and ignores further data
    OTA_STATE_ACTIVE,   //While reading data and writing storage
    //OTA_STATE_IGNORE    //On any error, it goes to i
}ota_state_t;


static struct{
    
    TaskHandle_t ota_task_handle;
    QueueHandle_t ota_queue;
    ota_state_t state;
    bool validation_pending;        //It will stop the ota_task from progressing
    bool image_header_was_checked;
    //char response_buffer[MAX_HTTP_RECV_BUFFER];
    //int data_len;
     //TimerHandle_t timer;    
}ota_service_state={0};


// ota_component.c
typedef struct {
    ota_data_event_t event;
    const char *data;
    size_t len;
} ota_event_msg_t;



DEFINE_EVENT_ADAPTER(OTA_SERVICE);

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

static void print_sha256 (const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
}

static void infinite_loop(void)
{
    int i = 0;
    ESP_LOGI(TAG, "When a new firmware is available on the server, press the reset button to download it");
    while(1) {
        ESP_LOGI(TAG, "Waiting for a new firmware ... %d", ++i);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}











    



static bool is_it_last_invalid_app(const char *new_version){

    const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
    if (last_invalid_app == NULL) {
        ESP_LOGI(TAG, "No invalid app partition found");
        return false;
    }

    esp_app_desc_t invalid_app_info;
    esp_err_t err = esp_ota_get_partition_description(last_invalid_app, &invalid_app_info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not read description of last invalid app partition (err=%s)",
                 esp_err_to_name(err));
        return false;   // generic read failure
    }

    ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);

    if (strcmp(invalid_app_info.version, new_version) == 0) {
        ESP_LOGW(TAG, "New version is the same as invalid version (%s). Skipping OTA.",
                 invalid_app_info.version);
        return true;  // special case
    }

    return false;
}


static bool is_firmware_updated(char* running_version,char* new_version){

    bool ret=false;
    
    int comparison = strcmp(new_version, running_version);

    if (comparison > 0) {
        ret = true;
    }

    return ret;
}


static void ota_task(void *pvParameter){
    esp_err_t err=0;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    
    ESP_LOGI(TAG, "Starting OTA example task");

    ota_service_state.ota_task_handle=xTaskGetCurrentTaskHandle();

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();


    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08"PRIx32", but running from offset 0x%08"PRIx32,
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08"PRIx32")",
             running->type, running->subtype, running->address);

    


    ota_state_t next_state=0;
    ota_event_msg_t msg;
    int binary_file_length=0;
    while (xQueueReceive(ota_service_state.ota_queue, &msg, portMAX_DELAY)) {
        
        
        switch(ota_service_state.state){

            case OTA_STATE_IDLE:

                if(msg.event==OTA_EVENT_DATA_ARRIVAL_STARTING_EVENT){
                    update_partition = esp_ota_get_next_update_partition(NULL);
                    if(update_partition!=NULL)
                        next_state=OTA_STATE_ACTIVE;
                }

                break;

            case OTA_STATE_ACTIVE:

                if(msg.event==OTA_EVENT_DATA_PACKET_ARRIVED_EVENT){
                    if (ota_service_state.image_header_was_checked == false) {
                        esp_app_desc_t new_app_info;
                        if (msg.len > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)){
                            // check current version with downloading
                            uint8_t* data=(uint8_t*)msg.data;
                            memcpy(&new_app_info, &data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                            ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                            esp_app_desc_t running_app_info;
                            if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                                ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
                            }

                            if(is_firmware_updated(running_app_info.version,new_app_info.version)==false){
                                next_state=OTA_STATE_IDLE;
                                break;
                            }

                            if(is_it_last_invalid_app(new_app_info.version)==true){
                                next_state=OTA_STATE_IDLE;
                                break;
                            }

                            ota_service_state.image_header_was_checked = true;

                            err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                            if (err != ESP_OK) {
                                ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                                //Go back to idle
                                next_state=OTA_STATE_IDLE;
                                //Some cleanup if required
                                
                                break;
                                //task_fatal_error();
                            }
                            ESP_LOGI(TAG, "esp_ota_begin succeeded");
                        } 
                        //The header must be read as a whole not chunks (needs improvement in future). So consider it fail if data_read<header size
                        else {
                            ESP_LOGE(TAG, "received package is not fit len");
                            next_state=OTA_STATE_IDLE;
                            //Some cleanup if required
                            
                            break;
                        }
                    }
                    
                    //If the break statement didnt run it means data read > header size so write as ota
                    //So this part of code is unconditional because the code above it will cause a break if some problem and so it will  not execute
                    err = esp_ota_write( update_handle, msg.data, msg.len);
                    if (err != ESP_OK) {
                        esp_ota_abort(update_handle);
                        next_state=OTA_STATE_IDLE;
                        break;
                    }
                    binary_file_length += msg.len;
                    ESP_LOGD(TAG, "Written image length %d", binary_file_length);
                    // Write to partition
                    // Check app version when enough data accumulated
                    break;
                }

            else if(msg.event==OTA_EVENT_DATA_COMPLETION_EVENT){
                   
                    err = esp_ota_end(update_handle);
                    if (err != ESP_OK) {
                        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
                            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
                        } else {
                            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
                        }
                        
                        next_state=OTA_STATE_IDLE;
                        break;
                        
                    }

                    err = esp_ota_set_boot_partition(update_partition);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
                        next_state=OTA_STATE_IDLE;
                        break;
                    }
                    ESP_LOGI(TAG, "Prepare to restart system!");
                    esp_restart();
                    return ;
                }

            
                break;

            default:
                break;
        }


        ota_service_state.state=next_state;
        //give back the buffer
        if(msg.event==OTA_EVENT_DATA_PACKET_ARRIVED_EVENT){
            OTA_SERVICE_post_event(OTA_SERVICE_ROUTINE_EVENT_DATA_BUFFER_USED,(const void*)&msg.data,sizeof(char*));
        }

    }
        //Wait till either signal arrives or time expires
     
        
    
}



esp_err_t ota_service_data_event(ota_data_event_t event, const char *data, size_t len)
{
    ota_event_msg_t msg = { .event = event, .data = data, .len = len };
    if (xQueueSend(ota_service_state.ota_queue, &msg, 0) != pdTRUE)
        return ESP_FAIL;
    return ESP_OK;
}



esp_err_t ota_set_valid(bool valid){
    if(valid){
        esp_ota_mark_app_valid_cancel_rollback();

    }
    else
        esp_ota_mark_app_invalid_rollback_and_reboot();
    
    return ESP_OK;
}


esp_err_t ota_service_init(){
 
    
    ota_service_state.ota_queue = xQueueCreate(3, sizeof(ota_event_msg_t));

    if(ota_service_state.ota_queue==NULL)
        return ESP_FAIL;

 
    //Task creation at end so that the client handle and semaphore are created before it
    BaseType_t ret;
    ret=xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
    if(ret==pdFAIL)
        return ESP_FAIL;

   
    OTA_SERVICE_register_event(OTA_SERVICE_ROUTINE_EVENT_VALIDATION_PENDING,NULL,NULL);
    //It will inform when the data buffer provided to it is used and now free
    OTA_SERVICE_register_event(OTA_SERVICE_ROUTINE_EVENT_DATA_BUFFER_USED,NULL,NULL);


    
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGE(TAG, "Firmware verification pending ...");
            ota_service_state.validation_pending=true;
            //If validation pending then post event
            OTA_SERVICE_post_event(OTA_SERVICE_ROUTINE_EVENT_VALIDATION_PENDING,NULL,0);
        }
    }

 
    return ESP_OK;
    
}