#include "esp_event.h"
#include "esp_log.h"
#include "softap_event_adapter.h"


static const char *TAG = "softap_adapter";
static esp_event_loop_handle_t* main_loop=NULL;
static esp_event_loop_handle_t* exception_loop=NULL;

ESP_EVENT_DEFINE_BASE(SOFTAP_EVENT_BASE);
ESP_EVENT_DEFINE_BASE(SOFTAP_EXCEPTION_BASE);

/* This is called from your softap_init() or similar to give adapter the context */
esp_err_t softap_event_adapter_init(){

    esp_err_t ret=0;
    

    ret|=event_context_register_routine_event(SOFTAP_EVENT_BASE, SOFTAP_EVT_CLIENT_CONNECTED);
    
    ret|=event_context_register_routine_event(SOFTAP_EVENT_BASE, SOFTAP_EVT_CLIENT_DISCONNECTED);
    ret|=event_context_register_exception_event(SOFTAP_EXCEPTION_BASE, SOFTAP_EXCP_INIT_FAIL);
    ret|=event_context_register_exception_event(SOFTAP_EXCEPTION_BASE, SOFTAP_EXCP_CONFIG_FAIL);
    ret|=event_context_register_exception_event(SOFTAP_EXCEPTION_BASE, SOFTAP_EXCP_DNS_FAIL);
    ret|=event_context_register_exception_event(SOFTAP_EXCEPTION_BASE, SOFTAP_EXCP_AP_CHANNEL_FAIL);
    ret|=event_context_register_exception_event(SOFTAP_EXCEPTION_BASE,SOFTAP_EXCP_IPCFG_FAIL);
    ret|=event_context_register_exception_event(SOFTAP_EXCEPTION_BASE,SOFTAP_EXCP_DHCP_FAIL );

    return ret;

}

/*-------------------------------------------------------
 * Post a normal operational event to the main loop
 *------------------------------------------------------*/
esp_err_t softap_post_event(softap_event_id_t id, void *data, size_t len)
{
    
    return event_context_post_event(SOFTAP_EVENT_BASE,id,data,len);
}

/*-------------------------------------------------------
 * Post an exception/error event to the exception loop
 *------------------------------------------------------*/
esp_err_t softap_post_exception(softap_exception_id_t id, void *data, size_t len)
{
    return event_context_post_exception(SOFTAP_EXCEPTION_BASE,id,data,len);
}
