#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "event_system_adapter.h"
#include "sync_manager.h"
#include "softap_service.h"
#include "routine_event_handler.h"
#include "espnow_discovery.h"
#include "http_server.h"
#include "ota_service.h"
#include "esp_now_transport.h"
#include "message_codec.h"
#include "wait_signal_bits.h"
#include "linear_actuator.h"

#include "gui_interface.h"
#include "gui_op.h"
#include "lcd_device.h"


static const char* TAG="Routine";

#define     MAX_WIFI_CHANNEL        13
#define     DELEGATE_QUEUE_LENGTH   4

#define DELEGATE_ARG_MAX_SIZE 32   // tune as needed; small fixed buffer

typedef void (*delegate_func_t)(void *arg, size_t len);

typedef struct {
    delegate_func_t func;
    uint8_t arg_data[DELEGATE_ARG_MAX_SIZE];
    size_t arg_len;
} delegate_job_t;

static QueueHandle_t delegate_queue = NULL;


static TaskHandle_t delegate_task_handle=NULL;
static gui_interface_t* gui_interface;



static void delegated_to_task_restart_discovery_on_new_channel(void *arg, size_t len) {
    if (len != sizeof(uint8_t)) return;
    uint8_t channel = *(uint8_t *)arg;

    ESP_LOGI(TAG, "new channel %d", channel);
    esp_now_transport_deinit();

    vTaskDelay(pdMS_TO_TICKS(1000));
    wifi_set_channel(channel);

    gui_event_data_t evt_data={.string="Searching Home",
                                .val=channel};
    
    gui_interface->gui_inform(SYSTEM_SEARCHING_HOME_NODE,&evt_data);
    esp_now_transport_config_t config = { .wifi_channel = channel };
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_now_transport_init(&config);

    start_discovery();
}


static void delegated_to_task_restart_wifi_apsta(void *arg, size_t len) {
    if (len != sizeof(uint8_t)) return;
    uint8_t channel = *(uint8_t *)arg;

    wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(2000));
    wifi_init_softap(channel);
    sync_manager_signal_set(SYNC_EVENT_DISCOVERY_COMPLETE);

}



static void delegate_run_task(void *arg) {
    delegate_job_t job;
    while (1) {
        if (xQueueReceive(delegate_queue, &job, portMAX_DELAY)) {
            job.func(job.arg_data, job.arg_len);
        }
    }
}


static esp_err_t delegate_post(delegate_func_t func, const void *arg, size_t len) {
    if (len > DELEGATE_ARG_MAX_SIZE) {
        ESP_LOGE("DELEGATE", "Argument too large (%d > %d)", len, DELEGATE_ARG_MAX_SIZE);
        return ESP_ERR_INVALID_ARG;
    }

    delegate_job_t job = { .func = func, .arg_len = len };
    if (arg && len > 0) {
        memcpy(job.arg_data, arg, len);
    }

    if (xQueueSend(delegate_queue, &job, portMAX_DELAY) != pdPASS) {
        ESP_LOGE("DELEGATE", "Failed to post delegate job");
        return ESP_FAIL;
    }
    return ESP_OK;
}





static void routine_ota_service_events_handler(void *handler_arg,
                                    int32_t id,
                                    void *event_data){
    switch(id){

        case OTA_SERVICE_ROUTINE_EVENT_DATA_BUFFER_USED:
            //The address of a pointer was passed to post, so double pointer is required here
            char** buffer= (char**) event_data;
            //Return the buffer back to the http buffer pool otherwise it will be short of buffer
            http_server_return_chunk_buffer(*buffer);
            break;
        
        case OTA_SERVICE_ROUTINE_EVENT_VALIDATION_PENDING:
            ota_set_valid(true);


        default:
            break;
    }


                                    }



static void routine_discovery_events_handler (void *handler_arg,
                                    int32_t id,
                                    void *event_data){

    //a makeshift workaround to scan all the channels one by one
    static uint8_t channel=0;
    
    switch(id){

        case DISCOVERY_EVENT_DISCOVERY_COMPLETE:
            uint8_t* total_discovered_devices=(uint8_t*)event_data;
            ESP_LOGI(TAG,"discovered: %d",*total_discovered_devices);
            //If no devices discovered then try again with new channel
            if(*total_discovered_devices==0){
                channel++;
                if(channel>MAX_WIFI_CHANNEL){
                    channel=1;
                }

                
                //Since the procedure involves deiniting and initing espnow and has vtaskdelay, 
                //so delegate to a task instead of blocking the event handler

                
                
                delegate_post(delegated_to_task_restart_discovery_on_new_channel,(void*)&channel,sizeof(channel));
            }
            //if devices discovered then stop wifi which was initialized as station and now reinit as APSTA
            else{
                ESP_LOGI(TAG,"its over %d",channel);

                delegate_post(delegated_to_task_restart_wifi_apsta,(void*)&channel,sizeof(channel));
                gui_interface->gui_inform(SYSTEM_BOOT_DONE,NULL);
                                
            }
            break;
        default:
            break;



    }

}



static void routine_http_server_events_handler (void *handler_arg,
                                    int32_t id,
                                    void *event_data){

    //a makeshift workaround to scan all the channels one by one
    static uint8_t channel=0;
    http_chunk_event_data_t* evt_data=(http_chunk_event_data_t*)event_data;
    lock_system_lock_interface_t* lock_interface=lock_system_get_interface();
                                
    //The data nis send only on one event, otherwise it is NULL
    char* buf=NULL;
    size_t len=0;
    if(evt_data!=NULL){
        buf= evt_data->ptr;
        len=evt_data->length;
    }
                                        
    
    switch(id){

        case HTTP_SERVER_EVENT_FILE_TRANSFER_STARTED:
               ota_service_data_event(OTA_EVENT_DATA_ARRIVAL_STARTING_EVENT,NULL,0);
            
            break;
        case HTTP_SERVER_EVENT_FILE_CHUNK_ARRIVED:

                if(buf==NULL)
                    break;
                
                ota_service_data_event(OTA_EVENT_DATA_PACKET_ARRIVED_EVENT,buf,len);
                //ESP_LOGI(TAG,"chunk buffer evt %p",buf);
                
            
            break;
        case HTTP_SERVER_EVENT_FILE_TRASFER_FAILED:
                ESP_LOGI(TAG,"failure");
               
            
            break;
        case HTTP_SERVER_EVENT_FILE_TRANSFER_COMPLETE:
               ota_service_data_event(OTA_EVENT_DATA_COMPLETION_EVENT,NULL,0);
            
            break;

        case HTTP_SERVER_EVENT_GATE_OPEN_COMMAND:
                //Inform the gui to show gate open status
                lock_interface->set_lock_open();
                //Since the procedure involves deiniting and initing espnow and has vtaskdelay, 
                //so delegate to a task instead of blocking the event handler
                
            
             break;
        default:
            break;



    }

}



static void routine_message_codec_events_handler (void *handler_arg,
                                    int32_t id,
                                    void *event_data){


    lock_system_lock_interface_t* lock_interface=lock_system_get_interface();
    lock_system_lock_status_t lock_status;
    

    switch(id){

        case MESSAGE_SERVICE_ROUTINE_EVENT_COMMAND_OPEN_GATE:
            lock_interface->set_lock_open();
            break;

        case MESSAGE_SERVICE_ROUTINE_EVENT_COMMAND_CLOSE_GATE:
            lock_interface->set_lock_close();
            break;
        case MESSAGE_SERVICE_ROUTINE_EVENT_COMMAND_SEND_GATE_STATUS:
            lock_status=lock_interface->get_lock_status();
            uint8_t* mac_addr=(uint8_t*)event_data;
            message_codec_lock_status_t message_lock_status=lock_status;
            message_codec_send_status(mac_addr,message_lock_status);
            break;


        case MESSAGE_SERVICE_ROUTINE_EVENT_GATE_STATUS_ARRIVED:
            break;


        
        default:
            break;




    }

                                    }







void routine_event_handler (void *handler_arg,
                            esp_event_base_t base,
                            int32_t id,
                            void *event_data){

    

        if(base==DISCOVERY_SERVICE_ROUTINE_EVENT_BASE){
            ESP_LOGI(TAG,"routine discovery event");
            routine_discovery_events_handler(handler_arg,id,event_data);
        }
         
        else if(base==HTTP_SERVER_ROUTINE_EVENT_BASE){
            //ESP_LOGI(TAG,"routine discovery event");
            routine_http_server_events_handler(handler_arg,id,event_data);
        }

        else if(base==OTA_SERVICE_ROUTINE_EVENT_BASE){
            //ESP_LOGI(TAG,"routine discovery event");
            routine_ota_service_events_handler(handler_arg,id,event_data);
        }   

        else if(base==MESSAGE_CODEC_ROUTINE_EVENT_BASE){
               ESP_LOGI(TAG,"trasnport routine");
            routine_message_codec_events_handler(handler_arg,id,event_data);

        }

        

}



esp_err_t routine_handler_init(){

    if (delegate_queue == NULL) {
        delegate_queue = xQueueCreate(DELEGATE_QUEUE_LENGTH, sizeof(delegate_job_t));
        if (delegate_queue == NULL) {
            ESP_LOGE(TAG, "failed to create delegate queue");
            return ESP_FAIL;
        }
    }

    if (delegate_task_handle == NULL) {
        BaseType_t res = xTaskCreatePinnedToCore(
            delegate_run_task,
            "run delegated tasks",
            3072,          // stack size
            NULL,
            5,             // priority
            &delegate_task_handle,
            tskNO_AFFINITY // can pin to core 0 or 1 if you prefer
        );
        if (res != pdPASS) {
            ESP_LOGE(TAG, "failed to create delegate task");
            return ESP_FAIL;
        }
    }

    gui_interface  = gui_op_get_interface();

    return ESP_OK;
}