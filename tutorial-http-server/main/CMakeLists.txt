# In main/CMakeLists.txt

# Define source files for this component
set(COMPONENT_SRCS "wifi_ap.c" "main.c" "led_control.c" "http_server.c")

# Register the component
# REQUIRES: This tells CMake which other components this 'main' component depends on.
#           Without 'spiffs' here, the compiler won't know where to find 'esp_spiffs.h'.
idf_component_register(SRCS "${COMPONENT_SRCS}"
                       INCLUDE_DIRS "."
                       REQUIRES spiffs esp_netif nvs_flash esp_http_server esp_event espressif__led_strip esp_wifi) # <-- **THIS LINE IS KEY**
                       # I've added a few more common dependencies that are likely used
                       # in your main/http_server/led files for good measure,
                       # like esp_http_server, esp_event, and espressif__led_strip.

# Command to create the SPIFFS partition image from your data directory
# The first argument 'storage' must match the name in partitions.csv
# The second argument '../data' is the path to your data directory relative to main/
spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)