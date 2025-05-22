#include <stdio.h>
#include <string.h>
#include <sys/unistd.h> // For stat() (optional, but good for file info)
#include <sys/stat.h>   // For stat()
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h" // <<< New: Include SPIFFS header for file operations
#include "led_control.h"        // Our LED control functions

static const char *TAG = "Basic HTTP Server";

// Keep track of the LED state
static bool led_is_on = false;

// --- Helper function to serve files from SPIFFS ---
static esp_err_t serve_file_from_spiffs(httpd_req_t *req, const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    char chunk[1024];
    size_t read_bytes;
    int res = HTTP_ACL;

    // Determine content type based on file extension
    // This is important for the browser to correctly interpret the file
    if (strstr(filepath, ".html")) {
        httpd_resp_set_type(req, "text/html");
    } else if (strstr(filepath, ".css")) {
        httpd_resp_set_type(req, "text/css");
    } else if (strstr(filepath, ".js")) {
        httpd_resp_set_type(req, "application/javascript");
    } else if (strstr(filepath, ".ico")) {
        httpd_resp_set_type(req, "image/x-icon");
    } else {
        httpd_resp_set_type(req, "text/plain"); // Default for unknown types
    }

    do {
        read_bytes = fread(chunk, 1, sizeof(chunk), f);
        if (read_bytes == 0) {
            res = HPE_OK; // End of file
        } else {
            res = httpd_resp_send_chunk(req, chunk, read_bytes);
        }
    } while (read_bytes > 0 && res == HPE_OK);

    fclose(f);
    return res;
}

/* Handler for the root URI ("/") - serves index.html from SPIFFS */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving root URI from SPIFFS: /spiffs/index.html");
    return serve_file_from_spiffs(req, "/spiffs/index.html");
}

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

/* Handler for static files (CSS, JS) */
static esp_err_t static_file_handler(httpd_req_t *req) {
    // Construct the full path to the file in SPIFFS
    char filepath[64]; // Adjust size if your paths are longer
    // req->uri contains the requested path, e.g., "/style.css" or "/script.js"
    // Prepend "/spiffs" to access the file from the mounted filesystem
    //snprintf(filepath, sizeof(filepath), "/spiffs%s", req->uri);
    ESP_LOGI(TAG, "Serving static file: %s", filepath);
    return serve_file_from_spiffs(req, filepath);
}

// Function to start the web server
httpd_handle_t start_webserver() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 5; // Increased for more handlers (root, toggle, status, css, js)

    // Start the httpd server
    //ESP_LOGI(TAG, "Starting server on port: '%d'", config.uri_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Server started, registering URI handlers...");

        // Register URI handler for the root path (/)
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        // Register URI handler for the toggle button (/toggle)
        httpd_uri_t toggle_uri = {
            .uri       = "/toggle",
            .method    = HTTP_GET,
            .handler   = toggle_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &toggle_uri);

        // Register URI handler for getting status (/status)
        httpd_uri_t status_uri = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        // Register URI handler for /style.css
        httpd_uri_t style_css_uri = {
            .uri       = "/style.css",
            .method    = HTTP_GET,
            .handler   = static_file_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &style_css_uri);

        // Register URI handler for /script.js
        httpd_uri_t script_js_uri = {
            .uri       = "/script.js",
            .method    = HTTP_GET,
            .handler   = static_file_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &script_js_uri);

        return server;
    }
    ESP_LOGI(TAG, "Error starting server");
    return NULL;
}