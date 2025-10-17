#ifndef http_server_H
#define http_server_H

#include "esp_err.h"


    


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

#endif