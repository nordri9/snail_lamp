#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "wifi_ap.h"
#include "esp_http_server.h"


static const char *TAG = "Basic HTTP Server";

/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    const char* resp_str = "<h1>Hello World</h1>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;

}


static const httpd_uri_t hello_world = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};




httpd_handle_t start_webserver(){
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Server ok, registering the URI handlers...");
        httpd_register_uri_handler(server, &hello_world);
        // httpd_register_uri_handler(server, &echo);
        // httpd_register_uri_handler(server, &ctrl);
        // httpd_register_uri_handler(server, &any);
        return server;
    }
    ESP_LOGI(TAG, "Error starting server");
    return NULL;
}