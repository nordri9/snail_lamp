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



static const char *TAG = "main";





void app_main(void)
{
    printf("Hello tutorial!\n");

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    //printf("Event WIFI_EVENT_AP_STACONNECTED %d\n",WIFI_EVENT_AP_STACONNECTED);
    wifi_init_softap();
    
    /*
    httpd_handle_t server = NULL;
    server = start_webserver();
    */
    //start_webserver();
}
