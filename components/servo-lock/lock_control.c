#include  <stdio.h>
#include "esp_log.h"
#include "lock_control.h"
#include "iot_servo.h"

static const char* TAG="lock";

#if defined(CONFIG_LOCK_GPIO_18)
    #define LOCK_GPIO_PIN 18
#elif defined(CONFIG_LOCK_GPIO_19)
    #define LOCK_GPIO_PIN 19
#elif defined(CONFIG_LOCK_GPIO_21)
    #define LOCK_GPIO_PIN 21
#elif defined(CONFIG_LOCK_GPIO_22)
    #define LOCK_GPIO_PIN 22
#elif defined(CONFIG_LOCK_GPIO_23)
    #define LOCK_GPIO_PIN 23
#elif defined(CONFIG_LOCK_GPIO_25)
    #define LOCK_GPIO_PIN 25
#elif defined(CONFIG_LOCK_GPIO_26)
    #define LOCK_GPIO_PIN 26
#elif defined(CONFIG_LOCK_GPIO_27)
    #define LOCK_GPIO_PIN 27
#else
    #error "No lock GPIO pin selected"
#endif

static struct{
    float lock_close_angle;
    float lock_open_angle;
    lock_system_lock_status_t status;
    gate_node_lock_interface_t interface;
}lock_state={0};




static esp_err_t set_lock_open(){
    ESP_LOGI(TAG,"opening");
    esp_err_t ret=iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, lock_state.lock_open_angle);
    lock_state.status=LOCK_STATUS_OPEN;
    return ret;
}
static esp_err_t set_lock_close(){
    ESP_LOGI(TAG,"opening");
    esp_err_t ret=iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, lock_state.lock_close_angle);
    lock_state.status=LOCK_STATUS_CLOSED;
    return ret;

}

lock_system_lock_status_t get_lock_status(){

    return lock_state.status;
}



gate_node_lock_interface_t* lock_create(lock_config_t* config){
    servo_config_t servo_cfg = {
    .max_angle = 180,
    .min_width_us = 500,
    .max_width_us = 2500,
    .freq = 50,
    .timer_number = LEDC_TIMER_0,
    .channels = {
        .servo_pin = {LOCK_GPIO_PIN},
        .ch = {LEDC_CHANNEL_0 },
    },
    .channel_number = 1,
};
    
    ESP_LOGI(TAG,"initing servo");
    esp_err_t ret=iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);
    if(ret!=ESP_OK)
        return NULL;
    ESP_LOGI(TAG,"initing servo done");
    lock_state.lock_close_angle=config->lock_close_angle;
    lock_state.lock_open_angle=config->lock_open_angle;
    lock_state.interface.set_lock_close=set_lock_close;
    lock_state.interface.set_lock_open=set_lock_open;
    lock_state.interface.get_lock_status=get_lock_status;

    return &lock_state.interface;

}

