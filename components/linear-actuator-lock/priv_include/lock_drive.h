/* Lock Drive Header File */
#ifndef LOCK_DRIVE_H
#define LOCK_DRIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    LOCK_DIRECTION_FORWARD,
    LOCK_DIRECTION_REVERSE
} lock_direction_t;

// Function prototypes

/// @brief There will be two GPIOs passed, one for forward direction and one for reverse direction
/// @param gpio_num 
/// @param gpio_count 
/// @return 
esp_err_t lock_drive_init(uint8_t gpio_num[], size_t gpio_count);
/// @brief Drives the lock to open position with speed implemented via PWM
/// @param direction 
/// @param speed 
/// @return 
esp_err_t lock_drive(lock_direction_t direction, uint8_t speed);


/// @brief Set the lock drive to idle state. Required when direction change is needed
/// @param  
/// @return 
esp_err_t lock_drive_idle(void);



#endif // LOCK_DRIVE_H