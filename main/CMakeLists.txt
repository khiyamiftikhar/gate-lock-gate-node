idf_component_register(SRCS "routine_event_handler.c" "exception_handler.c" "gate-node.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp-now-comm peer-registry discovery_service message_service esp_wifi softap-service nvs_flash linear-actuator-lock gate-system-common)
