#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "led_control.h"

static const char *TAG = "SimpleWebServer";

static esp_err_t serve_file(httpd_req_t *req, const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    // Set content type by file extension
    if (strstr(filepath, ".html"))
    {
        httpd_resp_set_type(req, "text/html");
    }
    else if (strstr(filepath, ".css"))
    {
        httpd_resp_set_type(req, "text/css");
    }
    else if (strstr(filepath, ".js"))
    {
        httpd_resp_set_type(req, "application/javascript");
    }
    else
    {
        httpd_resp_set_type(req, "text/plain");
    }

    char chunk[1024];
    size_t read_bytes;
    do
    {
        read_bytes = fread(chunk, 1, sizeof(chunk), f);
        if (read_bytes > 0)
        {
            httpd_resp_send_chunk(req, chunk, read_bytes);
        }
    } while (read_bytes > 0);
    fclose(f);

    // Signal end of HTTP response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// "/" → index.html
static esp_err_t root_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/index.html");
}

// "/style.css" → style.css
static esp_err_t css_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/style.css");
}

// "/script.js" → script.js
static esp_err_t js_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/script.js");
}

// Keep track of the LED state
static bool led_is_on = false;

/* Handler for "/toggle" URI - toggles the LED */
static esp_err_t toggle_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received /toggle request");

    // Toggle the LED state
    led_is_on = !led_is_on;
    set_led_state(led_is_on); // Call the LED control function

    // Prepare JSON response
    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"new_state\":%d}", led_is_on ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for "/status" URI - provides current LED state */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received /status request");

    char resp_str[50];
    sprintf(resp_str, "{\"led_state\":%d}", led_is_on ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

// Start the web server
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP server start failed");
        return NULL;
    }

    // Root
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &root_uri);

    // Register URI handler for the toggle button (/toggle)
    httpd_uri_t toggle_uri = {
        .uri = "/toggle",
        .method = HTTP_GET,
        .handler = toggle_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &toggle_uri);

    // Register URI handler for getting status (/status)
    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &status_uri);

    // CSS
    httpd_uri_t css_uri = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = css_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &css_uri);

    // JS
    httpd_uri_t js_uri = {
        .uri = "/script.js",
        .method = HTTP_GET,
        .handler = js_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &js_uri);

    return server;
}
