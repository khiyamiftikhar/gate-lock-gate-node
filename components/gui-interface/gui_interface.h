#ifndef GUI_INTERFACE_H
#define GUI_INTERFACE_H


#include "esp_err.h"

#ifdef __cplusplus
    extern "C" {
 #endif



//The name is misleading as if it is some gui event.
//Actually it is some system event for gui to display info about

 typedef enum{
    SYSTEM_BOOT,


}gui_event_t;



typedef struct{

    const char* string;


}gui_event_data_t;


typedef struct{
    esp_err_t (*gui_inform)(gui_event_t event,gui_event_data_t* evt_data);
}gui_interface_t;





#ifdef __cplusplus
    }
    #endif

#endif