
#ifndef SOFTAP_SERVICE_H
#define SOFTAP_SERVICE_H


#include "esp_err.h"


#include "system_context.h"


esp_err_t wifi_init_softap();

esp_err_t set_wifi_channel(uint8_t channel);


#endif