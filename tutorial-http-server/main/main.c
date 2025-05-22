/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "wifi_ap.h"
#include "esp_http_server.h"
#include "http_server.h"
#include "led_control.h"

#include "esp_spiffs.h"

static const char *TAG = "main";


// --- SPIFFS Initialization Function ---
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",         // Mount point: files will be accessed via /spiffs/...
      .partition_label = "storage",   // Label from partitions.csv
      .max_files = 5,                 // Max files that can be open at the same time
      .format_if_mount_failed = true  // Format partition if mount fails
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ret;
}




void app_main(void)
{
    printf("Hello tutorial!\n");

    // Initialize SPIFFS
    ESP_ERROR_CHECK(init_spiffs());

    ESP_LOGI(TAG, "Initializing Wi-Fi in SoftAP mode...");
    wifi_init_softap(); // Call your Wi-Fi initialization function

    ESP_LOGI(TAG, "Starting HTTP web server...");
    httpd_handle_t server = start_webserver(); // Call the web server function

    if (server != NULL) {
        ESP_LOGI(TAG, "Web server started successfully!");
    } else {
        ESP_LOGE(TAG, "Failed to start web server!");
    }
    // Your main application logic continues here.
    // The web server will run in the background.
    // For example, you might have a loop for other tasks or simply let it run.
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}
