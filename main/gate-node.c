
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "softap_service.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now_transport.h"
#include "espnow_discovery.h"
#include "database_interface.h"
#include "discovery_timer.h"
#include "peer_registry.h"
#include "message_codec.h"
#include "linear_actuator.h"
#include "system_context.h"
#include "sync_manager.h"
#include "event_system_adapter.h"
#include "exception_handler.h"
#include "routine_event_handler.h"



#define     DISCOVERY_DURATION              6000    //ms
#define     DISCOVERY_INTERVAL              2000    //ms
#define     ESPNOW_ENABLE_LONG_RANGE        1
#define     MAX_WIFI_CHANNEL                13

static const char* TAG="main gate";
static uint8_t ESPNOW_CHANNEL=1;

static TaskHandle_t main_task_handle = NULL;


    

#define     HOME_DEVICE_ID          1
static const uint8_t home_node_mac[]={0x64,0xe8,0x33,0x88,0x22,0x38};


/*
static esp_err_t set_lock_close(){
    ESP_LOGI(TAG,"closed");
    return 0;
}

static esp_err_t set_lock_open(){
    ESP_LOGI(TAG,"open");
    return 0;
}

static lock_system_lock_status_t get_lock_status(){
    return LOCK_STATUS_CLOSED;
}*/



/* WiFi should start before using ESPNOW */

static void esp_flash_init(){
     esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

}

/// @brief 
/// @param total_devices_found 
static void discovery_completion_handler(uint8_t total_devices_found){

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t notify_result;
    
    uint32_t result=total_devices_found;
    notify_result = xTaskNotifyFromISR(main_task_handle, (uint32_t)result, 
                                      eSetValueWithOverwrite, 
                                      &xHigherPriorityTaskWoken);
    
    /*
    if (notify_result != pdPASS) {
        // Notification failed - set a backup flag
        process_failed_flag = true;
        backup_result = result;
    }
    */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    //If no devices found, then restart with a different channel
}
 
static void restart_discovery_with_new_channel(){


        ESPNOW_CHANNEL++;
        if(ESPNOW_CHANNEL>MAX_WIFI_CHANNEL)
            ESPNOW_CHANNEL=1;
        //Deinitialize
        ESP_LOGI(TAG,"new channel %d",ESPNOW_CHANNEL);
        esp_now_transport_deinit();
        //set the channel again
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
         
        esp_now_transport_config_t config={.wifi_channel=ESPNOW_CHANNEL};

        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_now_transport_init(&config);

        //Again start discovery
        start_discovery();
    

}



void app_main(void)
{
    
    esp_err_t ret=0;
    main_task_handle = xTaskGetCurrentTaskHandle();
    
    
    
    esp_flash_init();
    //event_context_init();

    sync_manager_init();
    

    
    event_system_adapter_init(routine_event_handler,system_exception_handler);
    //Init as wifi station initally
    routine_handler_init();

    wifi_station_init();

    
    
    //sync_manager_signal_wait(SYNC_EVENT_DISCOVERY_COMPLETE,true,portMAX_DELAY);
    //wifi_init_softap();
    //softap_event_adapter_init();


    esp_now_transport_config_t transport_config={.wifi_channel=ESPNOW_CHANNEL};

    //The objcts created but callbacks not assigned. will be assigned later
    ret=esp_now_transport_init(&transport_config);

    if(ret==ESP_FAIL){
        ESP_LOGI(TAG,"transport init failed");
        ESP_ERROR_CHECK(ret);
    }


    
    peer_registry_config_t registry_config={.max_peers=2};

    peer_registry_interface_t* peer_registry=peer_registry_init(&registry_config);

    ESP_LOGI(TAG,"check in main %d",peer_registry->peer_registry_exists_by_mac(home_node_mac));

    peer_registry->peer_registry_add_peer(HOME_DEVICE_ID,home_node_mac,"homenode");
    
    //espnow_transport->esp_now_transport_add_peer(home_node_mac);

    ESP_LOGI(TAG,"peer registry init done");

    if(peer_registry==NULL)
        ESP_LOGI(TAG,"peer registry init failed");
    

    


    config_espnow_discovery discovery_config;
    //Must be an instance because config contains a pointer to it and 
    //unlike timer, peer_registry, its instance is not provided by any source
    
    esp_now_trasnsport_discovery_package_t* discovery_interface=esp_now_transport_get_discovery_interface();
    //This interface struct contaains complete package required by message service
    
    database_interface_t database_interface = {.is_white_listed=peer_registry->peer_registry_exists_by_mac};

    discovery_config.database_interface=&database_interface;
    discovery_config.peer_manager_interface=&discovery_interface->peer_manager_interface;
    discovery_config.discovery_interface=&discovery_interface->discovery_interface;
    
    
    //Assign the discovery interface to the discovery member of discovery config
    discovery_config.discovery_duration=DISCOVERY_DURATION;
    discovery_config.discovery_interval=DISCOVERY_INTERVAL;
    
    ret=discovery_service_init(&discovery_config);

    ESP_LOGI(TAG,"discovery init init done");
    //Now since discovery interface is created and it returned the handlers. now thoose handlers will be assigned to callbacks

    /*These are the callbacks which the esp-now-comm components require to call on the event and now provided by this service component
    //These are set using the methods in the espnow_transport_interface because putting and then merely assigning
    as interface member wont work as the returned interface pointer is a copy of the original
    */

    
    //Message Service component
    //Assign the interface members required by the message service commponent
    message_codec_config_t message_codec_config;
    
    message_codec_config.database_interface=&database_interface;
    esp_now_trasnsport_msg_package_t* message_interface=esp_now_transport_get_msg_interface();
    message_codec_config.msg_interface=&message_interface->msg_interface;

    message_codec_init(&message_codec_config);

    //This is redndant and needs to be optimized. discovery component has the same innterface
    
    linear_lock_config_t linear_lock_config={.unlock_hold_duration=2000,    //ms
            
    };
    lock_system_lock_interface_t* lock=linear_lock_create(&linear_lock_config);
    
    
    
    

    
    start_discovery();

    //Wait till discovery completes before proceeding
    sync_manager_signal_wait(SYNC_EVENT_DISCOVERY_COMPLETE,true,portMAX_DELAY);

    discovery_interface->peer_manager_interface.esp_now_transport_add_peer(home_node_mac);

    //wifi_init_softap();
    uint32_t current_time_ms = (xTaskGetTickCount() * 1000) / configTICK_RATE_HZ;
    uint32_t previous_time=current_time_ms;
    //This while 1 will run at boot until channel is found
    while(1){
        current_time_ms = (xTaskGetTickCount() * 1000) / configTICK_RATE_HZ;
        uint32_t total_devices_found=0;

        if(current_time_ms-previous_time>1000000){
                start_discovery();
                previous_time=current_time_ms;
        }
    
       vTaskDelay(pdMS_TO_TICKS(200));
    }

    

 
}
