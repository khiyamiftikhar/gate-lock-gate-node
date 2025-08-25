
#include  <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "linear_actuator.h"




#define GPIO_OUTPUT_IO_0    CONFIG_MOTOR_DRIVER_D0
#define GPIO_OUTPUT_IO_1    CONFIG_MOTOR_DRIVER_D1
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

//Max commands in queue
#define QUEUE_SIZE          10

//The duration after the lock will be brough again to lock position after unlock is just a multiple
//of the lock hold duration. This is done so, bcz if it is a separarate time then another freertos timer will be required
//So now the Lock duration = (lock hold duration * 2)
#define             MULTIPLIER          2


static const char* TAG="linear lock";


static struct{
    uint32_t unlock_hold_duration;  //The duration for which linear actuator will supply the volatge to hold in the unlock position
    uint32_t unlock_duration;   //After this duration the actuator will again be powered to lock position
    TimerHandle_t timer;
    //These two are to serialize the users
    QueueHandle_t command_queue;
    TaskHandle_t command_queue_task;
    lock_system_lock_status_t status;
    gate_node_lock_interface_t interface;
}lock_state={0};

typedef enum{
    COMMAND_CLOSE_MOTOR,        //Run in close direction
    COMMAND_OPEN_MOTOR,         //Run in open direction
    COMMAND_IDLE_MOTOR,          //Motor driver in idle position
    COMMAND_LOCK_STATUS
}motor_command_t;

//static motor_command_t motor_command={0};



static esp_err_t set_motor_driver_idle(){
    //ESP_LOGI(TAG,"opening");
    esp_err_t ret=gpio_set_level(GPIO_OUTPUT_IO_0,0);
            ret=gpio_set_level(GPIO_OUTPUT_IO_1,0);

    return ret;
}

static esp_err_t set_motor_open(){
    ESP_LOGI(TAG,"opening");
    
    motor_command_t command;
    command=COMMAND_OPEN_MOTOR;
    esp_err_t ret=xQueueSend(lock_state.command_queue,&command,portMAX_DELAY);
    
    
    return ret;
}
static esp_err_t set_motor_close(){
    //ESP_LOGI(TAG,"closing");
    motor_command_t command;
    command=COMMAND_CLOSE_MOTOR;
    esp_err_t ret=xQueueSend(lock_state.command_queue,&command,portMAX_DELAY);
    
    
    return ret;

}

static void lock_timer_callback_handler(TimerHandle_t timer){

    motor_command_t command;
    
    //if(lock_state.status==LOCK_STATUS_OPEN){
        command=COMMAND_IDLE_MOTOR;
        xQueueSend(lock_state.command_queue,&command,portMAX_DELAY);
        //lock_state.status=LOCK_STATUS_CLOSING;      //not closed but will be
    /*}
    else{
        command=COMMAND_CLOSE_MOTOR;
        xQueueSend(lock_state.command_queue,&command,portMAX_DELAY);
        //set_motor_close();
        //xTimerStop(lock_state.timer,0);
    }
*/
    
}


lock_system_lock_status_t get_lock_status(){

    return lock_state.status;
}


static void lock_task(void* args){

    motor_command_t command;
    while(1){

        if(xQueueReceive(lock_state.command_queue,&command,portMAX_DELAY)==pdTRUE){

            switch (command){
                case COMMAND_CLOSE_MOTOR:
                        ESP_LOGI(TAG,"closing command");
                   // if (xTimerIsTimerActive(lock_state.timer) == pdFALSE) {
                        ESP_LOGI(TAG,"closing command exec");
                        set_motor_driver_idle();
                        gpio_set_level(GPIO_OUTPUT_IO_0,1);
                        lock_state.status=LOCK_STATUS_CLOSED;
                        xTimerStart(lock_state.timer,portMAX_DELAY);
                    //}
                    break;

                case COMMAND_OPEN_MOTOR:

                    //Only give open command if timer is already not running
                    //if (xTimerIsTimerActive(lock_state.timer) == pdFALSE) {
                        set_motor_driver_idle();
                        gpio_set_level(GPIO_OUTPUT_IO_1,1);
                        xTimerStart(lock_state.timer,portMAX_DELAY);
                     //   lock_state.status=LOCK_STATUS_OPEN;
                        
                     //Timer is running
                    //}
                    lock_state.status=LOCK_STATUS_OPEN;
                    break;

            case COMMAND_IDLE_MOTOR:

                    /*If timer is already running, means a previous
                        open command hasnt completed, so override it
                    */
                    if(lock_state.status==LOCK_STATUS_CLOSED){
                        set_motor_driver_idle();
                        lock_state.status=LOCK_STATUS_CLOSED_IDLE;
                        xTimerStop(lock_state.timer,portMAX_DELAY);
                    }
                    else if(lock_state.status==LOCK_STATUS_OPEN){
                        set_motor_driver_idle();
                        lock_state.status=LOCK_STATUS_OPENDED_IDLE;
                    }

                    else if(lock_state.status==LOCK_STATUS_OPENDED_IDLE){
                        //Stop the timer
                        xTimerStop(lock_state.timer,portMAX_DELAY);
                        //Start fresh with the closing
                        lock_state.status=LOCK_STATUS_CLOSING;
                        ESP_LOGI(TAG,"closing back");
                        set_motor_close();
                        //lock_state.status=LOCK_STATUS_OPENDED_IDLE;
                    }
                    break;
            default:
                    break;

            }
        }
    }

}



gate_node_lock_interface_t* linear_lock_create(linear_lock_config_t* config){

    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;  //Because also want to read the level
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);


    

    lock_state.command_queue=xQueueCreate(QUEUE_SIZE,sizeof(motor_command_t));
    ESP_ERROR_CHECK(lock_state.command_queue==NULL);
    xTaskCreate(lock_task,"lock task",4096,NULL,2,&lock_state.command_queue_task);
    ESP_ERROR_CHECK(lock_state.command_queue_task==NULL);
    
    lock_state.unlock_hold_duration=config->unlock_hold_duration;
    lock_state.unlock_duration=lock_state.unlock_hold_duration*MULTIPLIER;

    lock_state.timer = xTimerCreate(
        "lock_timer",
        pdMS_TO_TICKS(lock_state.unlock_hold_duration),
        pdTRUE,  // Auto-reload
        NULL,
        lock_timer_callback_handler
    );

    if (!lock_state.timer) {
        ESP_LOGI(TAG, "Failed to create discovery timer");
        
        return NULL;
    }
    lock_state.interface.set_lock_close=set_motor_close;
    lock_state.interface.set_lock_open=set_motor_open;
    lock_state.interface.get_lock_status=get_lock_status;

    return &lock_state.interface;

}



