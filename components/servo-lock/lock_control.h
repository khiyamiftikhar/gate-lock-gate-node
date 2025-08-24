
#ifndef LOCK_CONTROL_H
#define LOCK_CONTROL_H

#include "gate_node.h"      //It defines the lock interface




typedef struct{

    float lock_close_angle;
    float lock_open_angle;


}lock_config_t;


gate_node_lock_interface_t* lock_create(lock_config_t* config);



#endif