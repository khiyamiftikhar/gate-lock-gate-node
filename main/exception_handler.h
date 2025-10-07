#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include "esp_event.h"

void exception_handler (void *handler_arg,
                            esp_event_base_t base,
                            int32_t id,
                            void *event_data);

#endif