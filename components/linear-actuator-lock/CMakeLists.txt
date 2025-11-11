idf_component_register(SRCS linear_actuator.c
                        INCLUDE_DIRS .
                        REQUIRES message_service lock_interface
                        PRIV_REQUIRES esp_driver_gpio
                        )





