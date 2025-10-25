#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H


#include "esp_err.h"
#include "stdint.h"



//The below is  equivalent 
//ESP_EVENT_DECLARE_BASE(MY_MODULE_NAME_ROUTINE_EVENT_BASE);
//but this way it does not require to include the esp_event header
extern const char * const OTA_SERVICE_ROUTINE_EVENT_BASE;

//The below is  equivalent 
//ESP_EVENT_DECLARE_BASE(MY_MODULE_NAME_EXCEPTION_EVENT_BASE);
extern const char * const OTA_SERVICE_EXCEPTION_EVENT_BASE;




#define     OTA_SERVICE_ROUTINE_EVENT_VALIDATION_PENDING        1
#define     OTA_SERVICE_ROUTINE_EVENT_DATA_BUFFER_USED          2



typedef enum{

    OTA_EVENT_DATA_ARRIVAL_STARTING_EVENT,
    OTA_EVENT_DATA_PACKET_ARRIVED_EVENT,
    OTA_EVENT_DATA_COMPLETION_EVENT


}ota_data_event_t;






/// @brief Inform the compponent about the OTA data events
/// @param event 
/// @param data 
/// @param len 
/// @return
//ota_data_event(OTA_EVENT_DATA_ARRIVAL_STARTING_EVENT, NULL, 0);
//ota_data_event(OTA_EVENT_DATA_PACKET_ARRIVED_EVENT, buffer, len);
//ota_data_event(OTA_EVENT_DATA_COMPLETION_EVENT, NULL, 0); 
esp_err_t ota_service_data_event(ota_data_event_t event, const char *data, size_t len);


/// @brief Called at boot to initalize the component
/// @return 
esp_err_t ota_service_init();











#endif