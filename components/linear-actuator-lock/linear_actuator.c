/**
 * @file linear_lock.c
 * @brief Linear actuator-based lock control system with PWM motor control
 * 
 * OVERVIEW:
 * This module implements a state machine for controlling a linear actuator lock mechanism.
 * It uses a dual-PWM motor driver to move the lock between open and closed positions with
 * smooth acceleration/deceleration profiles.
 * 
 * OPERATION:
 * - Lock movement is controlled via a command queue to serialize requests
 * - A FreeRTOS timer triggers PWM updates at regular intervals (100ms)
 * - Speed ramps up from 20% to 50%, then ramps down back to 0% for smooth motion
 * - When opening, the lock holds position briefly to prevent spring-back
 * - Motor driver uses two PWM channels (forward/reverse) for bidirectional control
 * 
 * STATE MACHINE:
 * - CLOSED/OPENED: Idle states, ready to accept commands
 * - CLOSING/OPENING: Active movement states with speed ramping
 * - Commands can interrupt ongoing movements by transitioning through idle
 * 
 * TIMING:
 * - Timer period: 100ms (updates PWM every 100ms during movement)
 * - Speed increment: ±20% per update (±10% during deceleration)
 * - Unlock hold duration: Configurable, prevents spring-back when open
 * 
 * @note Motor driver must be initialized via lock_drive_init() before use
 * @note Only one lock operation can be active at a time (queue serialization)
 */


#include  <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "linear_actuator.h"
#include "lock_drive.h"




#define GPIO_OUTPUT_IO_0    CONFIG_MOTOR_DRIVER_D0
#define GPIO_OUTPUT_IO_1    CONFIG_MOTOR_DRIVER_D1
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))


#define QUEUE_SIZE          10

// The duration after the lock will be brought again to lock position after unlock is just a multiple
// of the lock hold duration. This is done so, because if it is a separate time then another FreeRTOS timer will be required
// So now the Lock duration = (lock hold duration * MULTIPLIER)
#define             MULTIPLIER          2


static const char* TAG="linear lock";

/**
 * @brief Motor control commands for internal queue communication
 */
typedef enum{
    COMMAND_CLOSE_MOTOR,        // Run motor in close direction
    COMMAND_OPEN_MOTOR,         // Run motor in open direction
    COMMAND_IDLE_MOTOR,         // Set motor driver to idle (coast/brake)
    COMMAND_LOCK_STATUS         // Query current lock status
}motor_command_t;


/**
 * @brief Detailed internal lock states (more granular than public interface)
 * 
 * These states track not just position but also movement phase:
 * - Static states: CLOSED, OPENED, IDLE
 * - Transitional states: CLOSING, OPENING (active movement)
 * - Idle variants: CLOSED_IDLE, OPENED_IDLE (currently unused)
 */
typedef enum{
    LOCK_STATUS_CLOSED,         // Lock is fully closed and idle
    LOCK_STATUS_OPENED,         // Lock is fully open and idle
    LOCK_STATUS_CLOSED_IDLE,    // Reserved: closed position, motor idle
    LOCK_STATUS_OPENED_IDLE,    // Reserved: open position, motor idle
    LOCK_STATUS_CLOSING,        // Actively moving toward closed position
    LOCK_STATUS_OPENING,        // Actively moving toward open position
    LOCK_STATUS_IDLE,           // Motor idle, position unknown
}lock_internal_state_t;

/**
 * @brief Global state structure for the lock system
 * 
 * Contains all runtime state, timing parameters, synchronization primitives,
 * and current motion parameters. This structure is private to this module.
 */
static struct{
    uint32_t unlock_hold_duration;      // Duration (ms) to hold motor powered in unlock position
    uint32_t unlock_duration;           // Total unlock cycle duration (hold_duration * MULTIPLIER)
    TimerHandle_t timer;                // FreeRTOS timer for PWM updates during movement
    
    // Queue-based serialization to prevent concurrent lock operations
    QueueHandle_t command_queue;        // Command queue for lock operations
    TaskHandle_t command_queue_task;    // Handle to the lock control task
    
    lock_internal_state_t status;       // Current detailed lock state
    lock_system_lock_interface_t interface;  // Public interface structure
    
    // Motion control parameters (modified only within lock_task)
    motor_command_t current_command;    // Currently executing command
    lock_direction_t current_direction; // Current motor direction (FORWARD/REVERSE)
    uint8_t speed;                      // Current PWM speed (0-100%)
    int speed_increment;                // Speed change per timer tick (can be negative)
}lock_state={0};


/**
 * @brief Set motor driver to idle state (both PWM channels off)
 * 
 * Stops both forward and reverse PWM outputs, allowing motor to coast or brake
 * depending on driver configuration.
 * 
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t set_motor_driver_idle(){
    esp_err_t ret=gpio_set_level(GPIO_OUTPUT_IO_0,0);
            ret=gpio_set_level(GPIO_OUTPUT_IO_1,0);

    return ret;
}

/**
 * @brief Queue a command to open the lock
 * 
 * Sends COMMAND_OPEN_MOTOR to the command queue. The actual motor control
 * happens asynchronously in lock_task().
 * 
 * @return ESP_OK if command queued successfully
 * @note Blocks indefinitely if queue is full (portMAX_DELAY)
 */
static esp_err_t set_motor_open(){
    ESP_LOGI(TAG,"opening");
    
    motor_command_t command;
    command=COMMAND_OPEN_MOTOR;
    esp_err_t ret=xQueueSend(lock_state.command_queue,&command,portMAX_DELAY);
    
    return ret;
}

/**
 * @brief Queue a command to close the lock
 * 
 * Sends COMMAND_CLOSE_MOTOR to the command queue. The actual motor control
 * happens asynchronously in lock_task().
 * 
 * @return ESP_OK if command queued successfully
 * @note Blocks indefinitely if queue is full (portMAX_DELAY)
 */
static esp_err_t set_motor_close(){
    motor_command_t command;
    command=COMMAND_CLOSE_MOTOR;
    esp_err_t ret=xQueueSend(lock_state.command_queue,&command,portMAX_DELAY);
    
    return ret;
}

/**
 * @brief Timer callback for PWM speed updates during lock movement
 * 
 * Called every 100ms during active lock movement. Re-queues the current command
 * to trigger the next speed increment/decrement step in the state machine.
 * This creates a smooth acceleration/deceleration profile.
 * 
 * @param timer Handle to the timer that fired (unused)
 * 
 * @note The timer is started by lock_task when movement begins
 * @note Each timer fire advances the speed ramp by one step
 */
static void lock_timer_callback_handler(TimerHandle_t timer){
    motor_command_t command;
    
    ESP_LOGE(TAG, "TIMER fired, status=%d", lock_state.status); 
    command=lock_state.current_command;
    xQueueSend(lock_state.command_queue,&command,portMAX_DELAY);
}

/**
 * @brief Get current lock status for external interface
 * 
 * Translates internal detailed state to simplified public status enum.
 * 
 * @return LOCK_INTERFACE_LOCK_STATUS_CLOSE if locked
 *         LOCK_INTERFACE_LOCK_STATUS_OPEN if unlocked
 *         LOCK_INTERFACE_LOCK_STATUS_UNDEFINED for transitional states
 */
lock_system_lock_status_t get_lock_status(){
    lock_system_lock_status_t lock_status;
    switch(lock_state.status){

        case LOCK_STATUS_CLOSED:
            lock_status=LOCK_INTERFACE_LOCK_STATUS_CLOSE;
            break;
        case LOCK_STATUS_OPENED:
            lock_status=LOCK_INTERFACE_LOCK_STATUS_OPEN;
            break;
        default:
            lock_status=LOCK_INTERFACE_LOCK_STATUS_UNDEFINED;
            break;
    }
    return lock_status;
}


/**
 * @brief Main lock control task - processes commands and manages state machine
 * 
 * This task implements the lock state machine with speed ramping:
 * 
 * SPEED PROFILE:
 * - Starts at 20% duty cycle
 * - Ramps up by +20% per 100ms until reaching 50%
 * - Then ramps down by -20% (or -10% for opening) until reaching 0%
 * - At 0%, motor goes idle and state changes to CLOSED/OPENED
 * 
 * STATE TRANSITIONS:
 * - CLOSED/OPENED -> OPENING/CLOSING: Begin movement from idle
 * - OPENING/CLOSING -> opposite: Stop, go idle, then reverse direction
 * - OPENING/CLOSING -> same: Continue speed ramp (ignore duplicate commands)
 * 
 * @param args Unused task parameter
 * 
 * @note Runs indefinitely, blocked on command_queue when idle
 * @note All state variables are modified only within this task (thread-safe)
 */
static void lock_task(void* args){
    motor_command_t command;
    
    while(1){
        // Block waiting for next command from queue
        if(xQueueReceive(lock_state.command_queue,&command,portMAX_DELAY)==pdTRUE){
            ESP_LOGI(TAG,"lock speed %d",lock_state.speed);
            
            switch (lock_state.status){

                /* ============================================================
                 * CLOSING STATE: Lock is actively moving toward closed position
                 * ============================================================ */
                case LOCK_STATUS_CLOSING:
                    switch (command){
                        case COMMAND_OPEN_MOTOR:
                            // Direction reversal requested - stop and switch to opening
                            ESP_LOGI(TAG,"opening command");
                            
                            // First set motor to idle to stop current movement
                            lock_drive_idle();
                            
                            // Initialize opening movement parameters
                            lock_state.status=LOCK_STATUS_OPENING;
                            lock_state.speed=20;                        // Start at 20%
                            lock_state.speed_increment=20;              // Ramp up by 20%
                            lock_state.current_direction=LOCK_DIRECTION_FORWARD;
                            lock_state.current_command=COMMAND_OPEN_MOTOR;
                            
                            // Start timer to begin speed ramping
                            xTimerStart(lock_state.timer,portMAX_DELAY);
                            break;

                        case COMMAND_CLOSE_MOTOR:
                            // Continue closing movement - update speed ramp
                            lock_state.speed=lock_state.speed+lock_state.speed_increment;
                            
                            if(lock_state.speed>50){
                                // Reached peak speed, start decelerating
                                lock_state.speed_increment=-20;
                            }
                            else if(lock_state.speed<=0){
                                // Reached minimum speed - movement complete
                                lock_drive_idle();
                                lock_state.status=LOCK_STATUS_CLOSED;
                                lock_state.speed=20;  // Reset for next movement
                                break;  // Don't restart timer
                            }

                            // Apply new speed and schedule next update
                            lock_drive(lock_state.current_direction,lock_state.speed);
                            xTimerStart(lock_state.timer,portMAX_DELAY);
                            break;
                        
                        default:
                            // Ignore other commands (already closing)
                            break;
                    }
                    break;
                    

                /* ============================================================
                 * OPENING STATE: Lock is actively moving toward open position
                 * ============================================================ */
                case LOCK_STATUS_OPENING:
                    switch (command){
                        case COMMAND_CLOSE_MOTOR:
                            // Direction reversal requested - stop and switch to closing
                            ESP_LOGI(TAG,"closing command");
                            
                            // First set motor to idle to stop current movement
                            lock_drive_idle();
                            
                            // Initialize closing movement parameters
                            lock_state.status=LOCK_STATUS_CLOSING;
                            lock_state.speed=20;                        // Start at 20%
                            lock_state.speed_increment=20;              // Ramp up by 20%
                            lock_state.current_direction=LOCK_DIRECTION_REVERSE;
                            lock_state.current_command=COMMAND_CLOSE_MOTOR;
                            
                            // Start timer to begin speed ramping
                            xTimerStart(lock_state.timer,portMAX_DELAY);
                            break;

                        case COMMAND_OPEN_MOTOR:
                            // Continue opening movement - update speed ramp
                            lock_state.speed=lock_state.speed+lock_state.speed_increment;
                            
                            if(lock_state.speed>50){
                                // Reached peak speed, start decelerating (slower for opening)
                                lock_state.speed_increment=-10;
                            }
                            else if(lock_state.speed<=0){
                                // Reached minimum speed - movement complete
                                lock_drive_idle();
                                lock_state.status=LOCK_STATUS_OPENED;
                                lock_state.speed=20;  // Reset for next movement
                                break;  // Don't restart timer
                            }
                            else if(lock_state.speed<=10){
                                // At low speed, hold position to prevent spring-back
                                // This keeps the actuator powered to resist the lock mechanism's spring
                                vTaskDelay(pdMS_TO_TICKS(lock_state.unlock_hold_duration));
                            }

                            // Apply new speed and schedule next update
                            lock_drive(lock_state.current_direction,lock_state.speed);
                            xTimerStart(lock_state.timer,portMAX_DELAY);
                            break;

                        default:
                            // Ignore other commands (already opening)
                            break;
                    }
                    break;
                    /* ============================================================
                 * OPENED/CLOSED STATES: Lock is idle, ready to accept commands
                 * ============================================================ */
                case LOCK_STATUS_OPENED:
                case LOCK_STATUS_CLOSED:
                    ESP_LOGI(TAG,"init state");
                    switch (command){
                        case COMMAND_OPEN_MOTOR:
                            // Start opening movement from idle
                            ESP_LOGI(TAG,"opening command");
                            
                            // Initialize opening parameters
                            lock_state.current_direction=LOCK_DIRECTION_FORWARD;
                            lock_state.current_command=COMMAND_OPEN_MOTOR;
                            lock_state.speed=20;                    // Start at 20%
                            lock_state.speed_increment=20;          // Ramp up by 20%
                            
                            // Begin movement and change state
                            lock_drive(lock_state.current_direction,lock_state.speed);
                            lock_state.status=LOCK_STATUS_OPENING;
                            
                            // Start timer for speed ramping
                            xTimerStart(lock_state.timer,portMAX_DELAY);
                            break;

                        case COMMAND_CLOSE_MOTOR:
                            // Start closing movement from idle
                            ESP_LOGI(TAG,"closing command");
                            
                            // Initialize closing parameters
                            lock_state.current_direction=LOCK_DIRECTION_REVERSE;
                            lock_state.current_command=COMMAND_CLOSE_MOTOR;
                            lock_state.speed=20;                    // Start at 20%
                            lock_state.speed_increment=20;          // Ramp up by 20%
                            
                            // Begin movement and change state
                            lock_drive(lock_state.current_direction,lock_state.speed);
                            lock_state.status=LOCK_STATUS_CLOSING;
                            
                            // Start timer for speed ramping
                            xTimerStart(lock_state.timer,portMAX_DELAY);
                            break;

                        default:
                            // Ignore other commands in idle state
                            break;
                    }
                    break;
            
                default:
                    // Unknown state - do nothing
                    break;
            }
        }
    }
}



/**
 * @brief Get pointer to the public lock interface
 * 
 * Returns the interface structure that external code uses to control the lock.
 * The interface contains function pointers for lock operations.
 * 
 * @return Pointer to lock interface structure
 */
lock_system_lock_interface_t* lock_system_get_interface(){
    return &lock_state.interface;
}

/**
 * @brief Initialize the linear lock system
 * 
 * Sets up GPIO, creates FreeRTOS resources, initializes the motor driver,
 * and returns the public interface for lock control.
 * 
 * INITIALIZATION STEPS:
 * 1. Configure GPIO pins for motor driver
 * 2. Create command queue for serialization
 * 3. Create lock control task
 * 4. Create timer for speed ramping
 * 5. Initialize lock_drive module
 * 6. Populate public interface with function pointers
 * 
 * @param config Configuration structure containing:
 *               - unlock_hold_duration: Time (ms) to hold lock in open position
 * 
 * @return Pointer to lock interface on success, NULL on failure
 * 
 * @note GPIO pins are defined by GPIO_OUTPUT_PIN_SEL macro
 * @note Timer period is fixed at 100ms
 * @note Task priority is 2, stack size is 4096 bytes
 */
lock_system_lock_interface_t* linear_lock_create(linear_lock_config_t* config){

    // Configure GPIO pins for motor driver output
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;      // No interrupts needed
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;      // Bidirectional (can read back levels)
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL; // Pins for motor control
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Create command queue for lock operations (10 commands max)
    lock_state.command_queue=xQueueCreate(QUEUE_SIZE,sizeof(motor_command_t));
    ESP_ERROR_CHECK(lock_state.command_queue==NULL);
    
    // Create lock control task (priority 2, 4KB stack)
    xTaskCreate(lock_task,"lock task",4096,NULL,2,&lock_state.command_queue_task);
    ESP_ERROR_CHECK(lock_state.command_queue_task==NULL);
    
    // Store timing configuration
    lock_state.unlock_hold_duration=config->unlock_hold_duration;
    lock_state.unlock_duration=lock_state.unlock_hold_duration*MULTIPLIER;

    // Create timer for PWM speed updates (100ms period, one-shot mode)
    lock_state.timer = xTimerCreate(
        "lock_timer",
        pdMS_TO_TICKS(100),         // 100ms period
        pdFALSE,                    // One-shot (not auto-reload)
        NULL,                       // No timer ID needed
        lock_timer_callback_handler
    );

    if (!lock_state.timer) {
        ESP_LOGI(TAG, "Failed to create discovery timer");
        return NULL;
    }
    
    // Populate public interface with function pointers
    lock_state.interface.set_lock_close=set_motor_close;
    lock_state.interface.set_lock_open=set_motor_open;
    lock_state.interface.get_lock_status=get_lock_status;

    // Initialize the lock_drive module with GPIO pin assignments
    uint8_t gpio_nums[2]={GPIO_OUTPUT_IO_0,GPIO_OUTPUT_IO_1};    
    lock_drive_init(gpio_nums,2);
    
    return &lock_state.interface;
}