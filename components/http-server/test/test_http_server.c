
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "http_server.h"
#include "esp_log.h"
static const char* TAG="test http server";


static http_server_config_t config={0};

//static void setUp() {
    
  

    
//}






TEST_CASE("HTTP SERVER: Start","[Unit Test: HTTP Server]"){

    config.max_connections=2;
    config.max_uris=8;
    config.port=80;

    http_server_init(&config);
   

}



