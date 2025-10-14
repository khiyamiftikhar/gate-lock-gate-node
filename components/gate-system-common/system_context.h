/*It contains the variables used by all the components of the systems
It is the lowest in heirarchy. All components will have a source that
will use them to register and then post events*/

#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include "stdint.h"
#include "esp_err.h"

#define EVT_DISCOVERY_DONE (1 << 0)
#define EVT_AP_READY       (1 << 1)


//Fwd decleration so that netif header file is not required
typedef struct esp_netif_obj  esp_netif_t;

//Fwd declare this handle. So the standard header file not required
//struct esp_event_loop_instance_t;
typedef void* esp_event_loop_handle_t; /**< a number that identifies an event with respect to a base */



//Fwd decleration so that event group headers are not required
struct EventGroupDef_t;
typedef struct EventGroupDef_t   * EventGroupHandle_t;

/* Function pointer type for any handler that fits esp_event_handler_t signature */

/*typedef void (*app_event_handler_t)(void *arg,
                                    esp_event_base_t base,
                                    int32_t id,
                                    void *data);
*/

//Just decleration like previous fwd decleration. no signature is provided, 
//because detail is not required in this context file. it stores and supplies and does not call
typedef const char*  esp_event_base_t; /**< unique pointer to a subsystem that exposes events */
typedef void*        esp_event_loop_handle_t; /**< a number that identifies an event with respect to a base */
typedef void (*esp_event_handler_t)(void* event_handler_arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void* event_data); /**< function called when an event is posted to the queue */
/* Initialize the loops */
//esp_err_t event_context_init(void);

/* Main app injects the handlers here */

esp_err_t system_context_set_eventgroup_handle(EventGroupHandle_t handle);
EventGroupHandle_t system_context_get_eventgroup_handle();
esp_err_t system_context_set_routine_handler(esp_event_handler_t handler);
esp_err_t system_context_set_exception_handler(esp_event_handler_t handler);
esp_event_handler_t system_context_get_routine_handler();
esp_event_handler_t system_context_get_exception_handler();

esp_err_t system_context_set_station_netif_obj(esp_netif_t* sta_netif);

esp_netif_t* system_context_get_station_netif_obj();


esp_err_t system_context_set_ap_netif_obj(esp_netif_t* ap_netif);
esp_netif_t* system_context_get_station_netif_obj();


#endif