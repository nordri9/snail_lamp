idf_component_register(SRCS "led_effects.c" "main.c" "webserver.c" "led_control.c")

# Specify all your web assets in the data directory
spiffs_create_partition_image(spiffs ../data FLASH_IN_PROJECT)