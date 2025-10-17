idf_component_register(SRCS http_server.c
                        INCLUDE_DIRS .
                        PRIV_REQUIRES esp_http_server json
                        EMBED_FILES "page/dashboard.html" "page/login.html" "page/ota.html" EMBED_FILES "page/dashboard.js" "page/login.js" "page/ota.js"
                        )