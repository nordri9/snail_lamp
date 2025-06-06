idf_component_register(SRCS "main.c" "webserver.c") # Remove EMBED_FILES "root.html"

# Specify all your web assets in the data directory
spiffs_create_partition_image(spiffs ../data FLASH_IN_PROJECT)