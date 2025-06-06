//#include <sys/param.h>

//#include "esp_event.h"
#include "esp_log.h"
//#include "esp_mac.h"

//#include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
//#include "dns_server.h"

#include"led_control.h"

static const char *TAG = "webserver";

// Helper function to send files from SPIFFS
static esp_err_t send_file_from_spiffs(httpd_req_t *req, const char *filepath, const char *content_type)
{
    char full_path[64]; // Buffer for full path, e.g., "/spiffs/index.html"
    FILE *fd = NULL;
    struct stat file_stat;

    // Construct the full path for SPIFFS
    snprintf(full_path, sizeof(full_path), "/spiffs%s", filepath);

    if (stat(full_path, &file_stat) == -1)
    {
        ESP_LOGE(TAG, "File not found: %s", full_path);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    fd = fopen(full_path, "r");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to open file : %s", full_path);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Serving file: %s with Content-Type: %s", full_path, content_type);
    httpd_resp_set_type(req, content_type);

    char *chunk = malloc(file_stat.st_size + 1); // Allocate buffer for the whole file
    if (!chunk)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for file content");
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
        return ESP_FAIL;
    }

    size_t read_bytes = fread(chunk, 1, file_stat.st_size, fd);
    if (read_bytes != file_stat.st_size)
    {
        ESP_LOGE(TAG, "Failed to read all bytes from file (%zu/%ld)", read_bytes, file_stat.st_size);
        free(chunk);
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file content");
        return ESP_FAIL;
    }
    chunk[file_stat.st_size] = '\0'; // Null-terminate for string operations if any, good practice

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

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
    .user_ctx = NULL // No user callback needed
};

static const httpd_uri_t css_uri = {
    .uri = "/style.css",
    .method = HTTP_GET,
    .handler = css_get_handler,
    .user_ctx = NULL};

static const httpd_uri_t js_uri = {
    .uri = "/script.js",
    .method = HTTP_GET,
    .handler = js_get_handler,
    .user_ctx = NULL};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root_uri); // Use the new root_uri
        httpd_register_uri_handler(server, &css_uri);
        httpd_register_uri_handler(server, &js_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }

    // Register URI handler for getting status (/status)
    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &status_uri);

    // Register URI handler for the toggle button (/toggle)
    httpd_uri_t toggle_uri = {
        .uri = "/toggle",
        .method = HTTP_GET,
        .handler = toggle_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &toggle_uri);

    return server;
}