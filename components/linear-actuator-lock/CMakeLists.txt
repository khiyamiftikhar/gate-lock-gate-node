idf_component_register(SRCS linear_actuator.c priv_src/lock_drive.c
                        INCLUDE_DIRS .
                        PRIV_INCLUDE_DIRS priv_include
                        REQUIRES message_service lock_interface
                        PRIV_REQUIRES esp_driver_gpio esp_timer esp_driver_ledc
                        )





