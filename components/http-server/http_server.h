#ifndef http_server_H
#define http_server_H

#include "esp_err.h"


    
//The below is  equivalent 
//ESP_EVENT_DECLARE_BASE(MY_MODULE_NAME_ROUTINE_EVENT_BASE);
//but this way it does not require to include the esp_event header
extern const char * const HTTP_SERVER_ROUTINE_EVENT_BASE;

//The below is  equivalent 
//ESP_EVENT_DECLARE_BASE(MY_MODULE_NAME_EXCEPTION_EVENT_BASE);
extern const char * const HTTP_SERVER_EXCEPTION_EVENT_BASE;


#define   HTTP_SERVER_EVENT_FILE_TRANSFER_STARTED       1
#define   HTTP_SERVER_EVENT_FILE_CHUNK_ARRIVED          2
#define   HTTP_SERVER_EVENT_FILE_TRASFER_FAILED         3
#define   HTTP_SERVER_EVENT_FILE_TRANSFER_COMPLETE      4

typedef enum {
    PROTOCOL_HTTP,          // Regular HTTP
    PROTOCOL_HTTPS          // Secure HTTPS
} server_protocol_t;

typedef struct {
    uint16_t port;                    // Which port to listen on (default: 80 for HTTP, 443 for HTTPS)
    server_protocol_t protocol;      // HTTP or HTTPS
    uint8_t max_uris;                // How many different URIs to support (default: 10)
    uint16_t max_connections;        // Max simultaneous clients (default: 4)

} http_server_config_t;






esp_err_t http_server_init(http_server_config_t* config);
esp_err_t http_server_return_chunk_buffer(char* buffer);

#endif