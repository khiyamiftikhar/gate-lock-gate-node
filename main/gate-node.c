#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now_transport.h"
#include "espnow_discovery.h"
#include "discovery_timer.h"
#include "peer_registry.h"
#include "gate_node.h"
#include "lock_control.h"



#define     ESPNOW_CHANNEL          1
#define     DISCOVERY_DURATION      5000    //ms
#define     DISCOVERY_INTERVAL      500    //ms
#define     ESPNOW_ENABLE_LONG_RANGE    1

static const char* TAG="main gate";

#define     HOME_DEVICE_ID          1
static const uint8_t home_node_mac[]={0xe4,0x65,0xb8,0x19,0xf0,0x2c};


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
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());
    ESP_ERROR_CHECK( esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    ESP_LOGI(TAG, "Device Mac is " MACSTR, MAC2STR(mac));

#if ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}

static void esp_flash_init(){
     esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

}

void app_main(void)
{
    esp_flash_init();
    wifi_init();
    

    esp_now_transport_config_t transport_config={.wifi_channel=ESPNOW_CHANNEL};

    //The objcts created but callbacks not assigned. will be assigned later
    esp_now_trasnsport_interface_t* espnow_transport=esp_now_transport_init(&transport_config);

    if(espnow_transport==NULL)
        ESP_LOGI(TAG,"transport init failed");
    
    peer_registry_config_t registry_config={.max_peers=2};

    peer_registry_interface_t* peer_registry=peer_registry_init(&registry_config);

    ESP_LOGI(TAG,"check in main %d",peer_registry->peer_registry_exists_by_mac(home_node_mac));

    peer_registry->peer_registry_add_peer(HOME_DEVICE_ID,home_node_mac,"homenode");

    ESP_LOGI(TAG,"peer registry init done");

    if(peer_registry==NULL)
        ESP_LOGI(TAG,"peer registry init failed");
    

    discovery_timer_implementation_t* timer_interface=timer_create(DISCOVERY_INTERVAL);


    if(timer_interface==NULL)
        ESP_LOGI(TAG,"timer init failed");
    
    ESP_LOGI(TAG,"timer init done");

    config_espnow_discovery discovery_config;
    //Must be an instance because config contains a pointer to it and 
    //unlike timer, peer_registry, its instance is not provided by any source
    discovery_comm_interface_t discovery_comm_interface;
    
    //Assign methods required by the discovery service component provided by the esp-now-comm component
    discovery_comm_interface.acknowledge_the_discovery=espnow_transport->esp_now_transport_send_discovery_ack;
    discovery_comm_interface.add_peer=espnow_transport->esp_now_transport_add_peer;
    discovery_comm_interface.send_discovery=espnow_transport->esp_now_transport_send_discovery;

    //Assign the discovery interface to the discovery member of discovery config
    discovery_config.discovery=&discovery_comm_interface;
    discovery_config.timer=&timer_interface->methods;
    //The white list interface member assigned to the peer registry appropriate method
    discovery_whitelist_interface_t white_list;
    white_list.is_white_listed=peer_registry->peer_registry_exists_by_mac;
    discovery_config.whitelist=&white_list;
    discovery_config.discovery_duration=DISCOVERY_DURATION;
    discovery_config.discovery_interval=DISCOVERY_INTERVAL;
    
    discovery_service_interface_t* discovery_handlers=discovery_service_init(&discovery_config);

    ESP_LOGI(TAG,"discovery init init done");
    //Now since discovery interface is created and it returned the handlers. now thoose handlers will be assigned to callbacks

    /*These are the callbacks which the esp-now-comm components require to call on the event and now provided by this service component
    //These are set using the methods in the espnow_transport_interface because putting and then merely assigning
    as interface member wont work as the returned interface pointer is a copy of the original
    */

    espnow_transport->callbacks.on_device_discovered=discovery_handlers->comm_callback_handler.process_discovery_callback;
    espnow_transport->callbacks.on_discovery_ack=discovery_handlers->comm_callback_handler.process_discovery_acknowledgement_callback;
    
    timer_interface->callback_handler=discovery_handlers->timer_callback_handler.timer_handler;
    


    //Message Service component
    //Assign the interface members required by the message service commponent
    gate_node_config_t gate_config;
    
    //This is redndant and needs to be optimized. discovery component has the same innterface
    node_white_list_interface_t node_white_list;
    node_white_list.is_in_whitelist=peer_registry->peer_registry_exists_by_mac;
    //Must be an instance because config contains a pointer to it and 
    //unlike timer, peer_registry, its instance is not provided by any source
    gate_config.list=&node_white_list;
    node_msg_interface_t msg_interface;
    msg_interface.send_msg=espnow_transport->esp_now_transport_send_data;
    //The remaining callbacks of the esp_now_comm to the deserving targets
    //So now esp_now_comm will invoke methods inside the message service sources
    
    lock_config_t lock_config={.lock_close_angle=0,
                                .lock_open_angle=90

    };
    gate_node_lock_interface_t* lock=lock_create(&lock_config);
    
    /*
    lock.get_lock_status=get_lock_status;
    lock.set_lock_close=set_lock_close;
    lock.set_lock_open=set_lock_open;
    */
    
    
    gate_config.msg=&msg_interface;
    gate_config.lock=lock;
    
    
    gate_node_service_interface_t* gate_node= gate_node_init(&gate_config);

    espnow_transport->callbacks.on_data_received=gate_node->msg_received_handler;
    
    start_discovery();


    while(1){
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

 
}
