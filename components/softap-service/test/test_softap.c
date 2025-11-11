
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "softap_service.h"
#include "esp_log.h"
static const char* TAG="test http server";



void setUp() {
    
    
    
}






TEST_CASE("SOFTAP: Start","[Unit Test: SOFTAP]"){

    wifi_station_init();

    wifi_stop();
    wifi_init_softap(6);
   

}



void tearDown(){



}