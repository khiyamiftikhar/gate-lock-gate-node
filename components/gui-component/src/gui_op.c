#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdbool.h"
#include "gui_op.h"


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



gpu_interface_t* gui_op_get_interface(){
    if(gui_op->init==true)
        return &gui_op->interface;
    else
        return NULL;
}




void gui_op_init(){


    gui_op.gui_op_queue = xQueueCreate(10, sizeof(gui_op_info_t));
    ESP_ERROR_CHECK(gui_op.gui_op_queue==NULL);

    BaseType_t  ret=xTaskCreate(gui_op.gui_op_task, "lvgl_notify", 4096, NULL, 5, NULL);
    ESP_ERROR_CHECK(ret!=pdTRUE);



}

