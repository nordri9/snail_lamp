#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"

static const char *TAG = "SimpleWebServer";

// Serve a file from SPIFFS
static esp_err_t serve_file(httpd_req_t *req, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    char chunk[1024];
    size_t read_bytes;

    httpd_resp_set_type(req, "text/html");

    do {
        read_bytes = fread(chunk, 1, sizeof(chunk), f);
        if (read_bytes > 0) {
            httpd_resp_send_chunk(req, chunk, read_bytes);
        }
    } while (read_bytes > 0);

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // Signal end of response
    return ESP_OK;
}

// Handler for "/"
static esp_err_t root_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Serving /spiffs/index.html");
    return serve_file(req, "/spiffs/index.html");
}

// Start the web server
httpd_handle_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        return server;
    }

    ESP_LOGE(TAG, "Failed to start server");
    return NULL;
}
