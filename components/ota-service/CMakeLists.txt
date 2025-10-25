idf_component_register(SRCS ota_service.c
                        INCLUDE_DIRS .
                        PRIV_REQUIRES app_update event-adapter sync-manager
)