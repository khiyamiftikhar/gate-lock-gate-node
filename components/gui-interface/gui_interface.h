#ifndef GUI_INTERFACE_H
#define GUI_INTERFACE_H


#include "esp_err.h"

#ifdef __cplusplus
    extern "C" {
 #endif

#define     GUI_MAX_STRING_SIZE     50

//The name is misleading as if it is some gui event.
//Actually it is some system event for gui to display info about

 typedef enum{
    SYSTEM_BOOTING,
    SYSTEM_BOOT_DONE,
    SYSTEM_SEARCHING_HOME_NODE,
    


}gui_event_t;



typedef struct{
    const char string[GUI_MAX_STRING_SIZE];
}gui_event_data_t;


typedef struct{
    esp_err_t (*gui_inform)(gui_event_t event,gui_event_data_t* evt_data);
}gui_interface_t;





#ifdef __cplusplus
    }
    #endif

#endif