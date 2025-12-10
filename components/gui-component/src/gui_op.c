#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdbool.h"
#include "ui_boot.h"
#include "ui_home.h"
#include "gui_op.h"



#define GUI_WAIT_TIME   50
static const char* TAG = "gpu op";



typedef struct{
    gui_event_t event;
    gui_event_data_t evt_data;

}gui_op_info_t;


static struct
{
    TaskHandle_t gui_op_task;
    QueueHandle_t gui_op_queue;
    bool init;
    gui_interface_t interface;
}gui_op={0};



gui_interface_t* gui_op_get_interface(){
    if(gui_op.init==true)
        return &gui_op.interface;
    else
        return NULL;
}



static void gui_op_task(){

    gui_op_info_t op_info={0};



    while(1){


        if(xQueueReceive(gui_op.gui_op_queue,&op_info,portMAX_DELAY)==pdTRUE){

            switch(op_info.event){


                case SYSTEM_BOOTING:

                    break;

                default:
                    break;












            }
        }



    }



}


esp_err_t gui_inform(gui_event_t event, gui_event_data_t *evt_data)
{
    gui_op_info_t op_info = {0};

    op_info.event = event;

    if (evt_data)
        op_info.evt_data = *evt_data;   // full struct copy

    BaseType_t ret = xQueueSend(
        gui_op.gui_op_queue,
        &op_info,
        pdMS_TO_TICKS(GUI_WAIT_TIME)
    );

    return (ret == pdTRUE) ? ESP_OK : ESP_FAIL;
}

void gui_op_init(){


    gui_op.gui_op_queue = xQueueCreate(10, sizeof(gui_op_info_t));
    ESP_ERROR_CHECK(gui_op.gui_op_queue==NULL);

    BaseType_t  ret=xTaskCreate(gui_op.gui_op_task, "lvgl_notify", 4096, NULL, 5, NULL);
    ESP_ERROR_CHECK(ret!=pdTRUE);

    gui_op.interface.gui_inform=gui_inform;
    gui_op.init=true;

}

