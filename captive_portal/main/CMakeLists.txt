idf_component_register(SRCS "main.c" "webserver.c"
                       EMBED_FILES "root.html")

spiffs_create_partition_image(spiffs ../data FLASH_IN_PROJECT)