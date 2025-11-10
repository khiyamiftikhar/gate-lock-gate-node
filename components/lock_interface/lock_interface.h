#ifndef LOCK_INTERFACE_H
#define LOCK_INTERFACE_H

#include "esp_err.h"

typedef enum{
    LOCK_INTERFACE_LOCK_STATUS_OPEN,
    LOCK_INTERFACE_LOCK_STATUS_CLOSE,
    LOCK_INTERFACE_LOCK_STATUS_UNDEFINED

}lock_system_lock_status_t;


typedef struct{
    esp_err_t (*set_lock_open)();
    esp_err_t (*set_lock_close)();
    lock_system_lock_status_t (*get_lock_status)();
}lock_system_lock_interface_t;



#endif