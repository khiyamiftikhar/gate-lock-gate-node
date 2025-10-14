
#ifndef SOFTAP_SERVICE_H
#define SOFTAP_SERVICE_H


#include "esp_err.h"


//The below is  equivalent 
//ESP_EVENT_DECLARE_BASE(MY_MODULE_NAME_ROUTINE_EVENT_BASE);
//but this way it does not require to include the esp_event header
extern const char * const SOFTAP_SERVICE_ROUTINE_EVENT_BASE;

//The below is  equivalent 
//ESP_EVENT_DECLARE_BASE(MY_MODULE_NAME_EXCEPTION_EVENT_BASE);
extern const char * const SOFTAP_SERVICE_EXCEPTION_EVENT_BASE;


typedef enum {
    SOFTAP_EXCP_INIT_FAIL,
    SOFTAP_EXCP_CONFIG_FAIL,
    SOFTAP_EXCP_DNS_FAIL,
    SOFTAP_EXCP_AP_CHANNEL_FAIL,
    SOFTAP_EXCP_IPCFG_FAIL,
    SOFTAP_EXCP_DHCP_FAIL

    // ...
} softap_exception_id_t;


/// @brief Start as station
/// @param  
void wifi_station_init(void);
/// @brief Call it after peer is discovered
/// @return 
esp_err_t wifi_stop();
/// @brief Init as softAP with the channel discovered
/// @param channel 
/// @return 
esp_err_t wifi_init_softap(uint8_t channel);
/// @brief 
/// @param channel 
/// @return 
esp_err_t wifi_set_channel(uint8_t channel);



#endif