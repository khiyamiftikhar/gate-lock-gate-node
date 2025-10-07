/*It contains the variables used by all the components of the systems
It is the lowest in heirarchy. All components will have a source that
will use them to register and then post events*/

#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include "esp_event.h"



//Fwd decleration so that netif header file is not required
typedef struct esp_netif_obj  esp_netif_t;




/* Function pointer type for any handler that fits esp_event_handler_t signature */
typedef void (*app_event_handler_t)(void *arg,
                                    esp_event_base_t base,
                                    int32_t id,
                                    void *data);


/* Initialize the loops */
esp_err_t event_context_init(void);

/* Main app injects the handlers here */
void event_context_set_routine_handler(app_event_handler_t handler);
void event_context_set_exception_handler(app_event_handler_t handler);

/* Components call these wrappers to register events without handler knowledge */
esp_err_t event_context_register_routine_event(esp_event_base_t base, int32_t id);
esp_err_t event_context_register_exception_event(esp_event_base_t base, int32_t id);

esp_err_t event_context_post_event(esp_event_base_t base, int32_t id,void *data, size_t len);
esp_err_t event_context_post_exception(esp_event_base_t base, int32_t id,void *data, size_t len);


esp_err_t set_station_netif_obj(esp_netif_t* sta_netif);

esp_netif_t* get_station_netif_obj();


esp_err_t set_ap_netif_obj(esp_netif_t* ap_netif);
esp_netif_t* get_station_netif_obj();


#endif