idf_component_register(SRCS "softap_service.c" "softap_event_adapter.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES esp_wifi wpa_supplicant gate-system-common)