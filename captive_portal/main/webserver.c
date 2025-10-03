// File: main/webserver.c

#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_http_server.h"

// Your existing header
#include "webserver.h"
// The NEW header for our API handlers
#include "web_handlers.h"

static const char *TAG = "webserver";

// --- File Serving Logic (Stays in webserver.c) ---

// Helper function to send files from SPIFFS
static esp_err_t send_file_from_spiffs(httpd_req_t *req, const char *filepath, const char *content_type)
{
    // ... This function remains exactly the same as before ...
    char full_path[64];
    FILE *fd = NULL;
    struct stat file_stat;

    snprintf(full_path, sizeof(full_path), "/spiffs%s", filepath);

    if (stat(full_path, &file_stat) == -1)
    {
        ESP_LOGE(TAG, "File not found: %s", full_path);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    fd = fopen(full_path, "r");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to open file : %s", full_path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Serving file: %s", full_path);
    httpd_resp_set_type(req, content_type);

    char *chunk = malloc(file_stat.st_size);
    if (!chunk)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for file content");
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
        return ESP_FAIL;
    }

    if (fread(chunk, 1, file_stat.st_size, fd) != file_stat.st_size)
    {
        ESP_LOGE(TAG, "Failed to read file content");
        free(chunk);
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File read error");
        return ESP_FAIL;
    }

    httpd_resp_send(req, chunk, file_stat.st_size);
    free(chunk);
    fclose(fd);
    return ESP_OK;
}

// HTTP GET Handler for index.html (root)
static esp_err_t index_get_handler(httpd_req_t *req)
{
    return send_file_from_spiffs(req, "/index.html", "text/html");
}

// HTTP GET Handler for style.css
static esp_err_t css_get_handler(httpd_req_t *req)
{
    return send_file_from_spiffs(req, "/style.css", "text/css");
}

// HTTP GET Handler for script.js
static esp_err_t js_get_handler(httpd_req_t *req)
{
    return send_file_from_spiffs(req, "/script.js", "application/javascript");
}

// HTTP Error (404) Handler
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// --- Webserver Start/Stop ---

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Error starting server!");
        return NULL;
    }

    // --- Register URI handlers ---
    ESP_LOGI(TAG, "Registering URI handlers");

    // Handlers for static files
    httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = index_get_handler};
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t css_uri = {.uri = "/style.css", .method = HTTP_GET, .handler = css_get_handler};
    httpd_register_uri_handler(server, &css_uri);

    httpd_uri_t js_uri = {.uri = "/script.js", .method = HTTP_GET, .handler = js_get_handler};
    httpd_register_uri_handler(server, &js_uri);

    // Handlers for the API (now in a separate function)
    register_api_uri_handlers(server);

    // Handler for 404 errors
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

    return server;
}