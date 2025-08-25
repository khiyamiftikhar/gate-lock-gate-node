#ifndef LINEAR_ACTUATOR_H
#define LINEAR_ACTUATOR_H

#include <stdint.h>
#include "gate_node.h"      //It defines the lock interface




typedef struct{
        //MILLISECONDS
    uint32_t unlock_hold_duration;  //The duration for which linear actuator will supply the volatge to hold in the unlock position
    
    //Not a separate parameter. i will just be multiple(double triple) of the unlock hold duration
    //uint32_t unlock_duration;   //After this duration the actuator will again be powered to lock position


}linear_lock_config_t;


gate_node_lock_interface_t* linear_lock_create(linear_lock_config_t* config);



#endif