#include "system_context.h"
#include "esp_log.h"


static const char *TAG = "system_ctx";

typedef struct{

    esp_event_loop_handle_t main_loop;      //For routine events
    esp_event_loop_handle_t exception_loop; //For exception events
    esp_netif_t *sta_netif;                 //Created by wifi soft ap but required by http server so shared from here
    esp_netif_t *ap_netif; 
    esp_event_handler_t s_routine_handler;      //handler for routine event , defined in main
    esp_event_handler_t s_exception_handler;    //handler for exception event , defined in main
    EventGroupHandle_t system_events_group;       //Main waits for events to proceed with inits that depend upon previous init

}system_context_t;

static system_context_t sys_context={0};


/*
void sysctx_signal_event(system_context_t *ctx, EventBits_t bits)
{
    xEventGroupSetBits(sys_context.system_events_group, bits);
}

void sysctx_wait_for_event(system_context_t *ctx, EventBits_t bits,uint32_t ticks)
{
    xEventGroupWaitBits(sys_context.system_events_group, bits, pdTRUE, pdFALSE,ticks);
}

esp_err_t event_context_post_event(esp_event_base_t base, int32_t id,void *data, size_t len){

    if (sys_context.main_loop == NULL) {
        ESP_LOGE(TAG, "Context or main loop not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_event_post_to(sys_context.main_loop, base, id, data, len, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post SOFTAP event %"PRId32 ": %s", id, esp_err_to_name(err));
    }
    return err;


}
esp_err_t event_context_post_exception(esp_event_base_t base, int32_t id,void *data, size_t len){


    if (sys_context.exception_loop == NULL) {
        ESP_LOGE(TAG, "Context or main loop not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_event_post_to(sys_context.exception_loop, base, id, data, len, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post SOFTAP event %"PRId32 ": %s", id, esp_err_to_name(err));
    }
    return err;




}


*/


/* Function pointer storage — set by main */




/* ---- Wrappers for components ---- */

/*
esp_err_t event_context_register_routine_event(esp_event_base_t base, int32_t id)
{
    if (!sys_context.main_loop || !sys_context.s_routine_handler)
        return ESP_ERR_INVALID_STATE;

    return esp_event_handler_instance_register_with(
        sys_context.main_loop, base, id, sys_context.s_routine_handler, NULL, NULL);
}

esp_err_t event_context_register_exception_event(esp_event_base_t base, int32_t id)
{
    if (!sys_context.exception_loop || !sys_context.s_exception_handler)
        return ESP_ERR_INVALID_STATE;

    return esp_event_handler_instance_register_with(
        sys_context.exception_loop, base, id, sys_context.s_exception_handler, NULL, NULL);
}
*/

/* ---- Setters used by main ---- */


esp_err_t system_context_set_routine_handler(esp_event_handler_t handler)
{
    if(handler==NULL)
        return ESP_FAIL;
    sys_context.s_routine_handler = handler;

    return ESP_OK;
}

esp_err_t system_context_set_exception_handler(esp_event_handler_t handler)
{
    if(handler==NULL)
        return ESP_FAIL;
    sys_context.s_exception_handler = handler;
    return ESP_OK;
}



esp_event_handler_t system_context_get_routine_handler()
{
    return sys_context.s_routine_handler;

}

esp_event_handler_t system_context_get_exception_handler()
{
 
    return sys_context.s_exception_handler;
}


/*
esp_err_t event_context_init(void)
{
    
    sys_context.system_events_group = xEventGroupCreate();
    esp_event_loop_args_t routine_args = {
        .queue_size = 16,
        .task_name = "routine_loop",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };
    esp_event_loop_args_t exc_args = {
        .queue_size = 8,
        .task_name = "exception_loop",
        .task_priority = 4,
        .task_stack_size = 3072,
        .task_core_id = tskNO_AFFINITY
    };

    ESP_ERROR_CHECK(esp_event_loop_create(&routine_args, &sys_context.main_loop));
    ESP_ERROR_CHECK(esp_event_loop_create(&exc_args, &sys_context.exception_loop));

    ESP_LOGI(TAG, "Event loops created");
    return ESP_OK;
}
*/

esp_err_t system_context_set_station_netif_obj(esp_netif_t* sta_netif){

    if(sta_netif==NULL)
        return ESP_FAIL;

    sys_context.sta_netif=sta_netif;

    return ESP_OK;

}

esp_netif_t* system_context_get_station_netif_obj(){

    return sys_context.sta_netif;

}

esp_err_t set_ap_netif_obj(esp_netif_t* ap_netif){

    if(ap_netif==NULL)
        return ESP_FAIL;

    sys_context.ap_netif=ap_netif;
    return ESP_OK;
}


esp_netif_t* get_ap_netif_obj(){

    return sys_context.ap_netif;

}