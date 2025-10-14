#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "event_system_adapter.h"
#include "sync_manager.h"
#include "softap_service.h"
#include "routine_event_handler.h"
#include "espnow_discovery.h"
#include "esp_now_transport.h"
#include "wait_signal_bits.h"

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



static void delegated_to_task_restart_discovery_on_new_channel(void *arg, size_t len) {
    if (len != sizeof(uint8_t)) return;
    uint8_t channel = *(uint8_t *)arg;

    ESP_LOGI(TAG, "new channel %d", channel);
    esp_now_transport_deinit();

    vTaskDelay(pdMS_TO_TICKS(1000));
    wifi_set_channel(channel);

    esp_now_transport_config_t config = { .wifi_channel = channel };
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_now_transport_init(&config);

    start_discovery();
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
                wifi_stop();
                sync_manager_signal_set(SYNC_BIT_WIFI_INIT_DONE);
                wifi_init_softap(channel);
                
            }
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

    return ESP_OK;
}