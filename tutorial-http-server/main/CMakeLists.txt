idf_component_register(SRCS "wifi.c" "main.c" "led_control.c"
                    PRIV_REQUIRES esp_wifi esp_http_server
                    INCLUDE_DIRS ".")
