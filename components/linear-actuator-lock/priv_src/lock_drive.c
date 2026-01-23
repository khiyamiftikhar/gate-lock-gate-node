#include <stdio.h>
#include <string.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

#include "lock_drive.h"


#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (5) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_FREQUENCY          (100) // Frequency in Hertz. Set frequency at 100Hz so 10 ms


static struct 
{

    uint8_t channel_forward;
    uint8_t channel_reverse;
    /* data */
}lock_drive_state={0};



static uint32_t speed_to_duty(uint8_t speed){

    //Speed is 0-100
    //Duty is 0-8191 for 13 bit resolution
    if(speed>100)
        speed=100;

    return (speed*8191)/100;
}

esp_err_t lock_drive_idle(void){

    ledc_stop(LEDC_MODE,lock_drive_state.channel_forward,0);
    return ledc_stop(LEDC_MODE,lock_drive_state.channel_reverse,0);
    
}



esp_err_t lock_drive(lock_direction_t direction, uint8_t speed){

    
    switch (direction)
    {
    case LOCK_DIRECTION_FORWARD:
        ledc_set_duty(LEDC_MODE,lock_drive_state.channel_forward,speed_to_duty(speed));
        ledc_update_duty(LEDC_MODE, lock_drive_state.channel_forward);
        break;
    
    case LOCK_DIRECTION_REVERSE:
        ledc_set_duty(LEDC_MODE,lock_drive_state.channel_reverse,speed_to_duty(speed));
        ledc_update_duty(LEDC_MODE, lock_drive_state.channel_reverse);
        break;
    
    default:
        break;
    }

    
    return ESP_OK;

}




/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2 (rev < 1.2), ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */

esp_err_t lock_drive_init(uint8_t gpio_num[], size_t gpio_count){
    // Prepare and then apply the LEDC PWM timer configuration
    
    if(gpio_count<2)
        return ESP_FAIL;
    

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    if(ledc_timer_config(&ledc_timer)!=0)
        return -1;


    // Prepare and then apply the LEDC PWM channel configuration
    //There are two modes high and low, and 8 channel for each
    //So channel member can have value 0-7 but speed_mode tells channel no of which device, low or high
    ledc_channel_config_t ledc_channel_0 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio_num[0],
        .duty           = 0,
        .hpoint         = 0,
    };

    ledc_channel_config_t ledc_channel_1 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio_num[1],
        .duty           = 0,
        .hpoint         = 0,
    };

    //ESP_LOGI(TAG,"before channel config");
    if(ledc_channel_config(&ledc_channel_0)!=0)
        return ESP_FAIL;
    if(ledc_channel_config(&ledc_channel_1)!=0)
        return ESP_FAIL;
    //ESP_LOGI(TAG,"after channel config");

    lock_drive_state.channel_forward = LEDC_CHANNEL_0;
    lock_drive_state.channel_reverse = LEDC_CHANNEL_1;
    

    //ESP_LOGI(TAG,"returning from pwm_line");

    return ESP_OK;
    
}