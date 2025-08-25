idf_component_register(SRCS "gate-node.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp-now-comm peer-registry discovery_service message_service esp_wifi wpa_supplicant nvs_flash linear-actuator-lock)
