#ifndef SOFTAP_EVENT_ADAPTER_H
#define SOFTAP_EVENT_ADAPTER_H

#include "esp_err.h"
#include "system_context.h"

/*Routine Event IDs*/

typedef enum {
    SOFTAP_EVT_CLIENT_CONNECTED,
    SOFTAP_EVT_CLIENT_DISCONNECTED,

} softap_event_id_t;



typedef enum {
    SOFTAP_EXCP_INIT_FAIL,
    SOFTAP_EXCP_CONFIG_FAIL,
    SOFTAP_EXCP_DNS_FAIL,
    SOFTAP_EXCP_AP_CHANNEL_FAIL,
    SOFTAP_EXCP_IPCFG_FAIL,
    SOFTAP_EXCP_DHCP_FAIL

    // ...
} softap_exception_id_t;

esp_err_t softap_event_adapter_init();
esp_err_t softap_post_event(softap_event_id_t id, void *data, size_t len);
esp_err_t softap_post_exception(softap_exception_id_t id, void *data, size_t len);





#endif
