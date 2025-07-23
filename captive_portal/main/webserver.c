// #include <sys/param.h>

// #include "esp_event.h"
#include "esp_log.h"
// #include "esp_mac.h"

// #include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
// #include "dns_server.h"

#include "led_control.h"
#include "led_effects.h"

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
static bool led_is_on = true;

/* Handler for "/status" URI - provides current LED state, hue, AND brightness */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received /status request");

    uint16_t current_hue = get_last_set_hue();
    uint8_t current_brightness = get_last_set_brightness(); // <<< NEW: Get last brightness

    char resp_str[150]; // Increased buffer size for hue and brightness
    sprintf(resp_str, "{\"led_state\":%d, \"current_hue\":%d, \"current_brightness\":%d}",
            led_is_on ? 1 : 0, current_hue, current_brightness); // <<< MODIFIED JSON
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for "/toggle" URI - toggles the LED */
static esp_err_t toggle_get_handler(httpd_req_t *req)

{
    ESP_LOGI(TAG, "Received /toggle request");

    // Before toggling, stop any running effects
    stop_led_effect();
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

/* Handler for setting LED color via hue */
static esp_err_t set_hue_get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    uint16_t hue = 0;

    // Get query string length and allocate memory for it
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    { // If there is a query string
        buf = (char *)malloc(buf_len);
        if (!buf)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for query buffer");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal server error");
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[10]; // Buffer for the 'hue' parameter value
            if (httpd_query_key_value(buf, "hue", param, sizeof(param)) == ESP_OK)
            {
                hue = atoi(param);
                ESP_LOGI(TAG, "Received hue: %d", hue);
                // Ensure LED is on before setting color

                stop_led_effect(); // <<< NEW: Stop effect when setting solid color

                if (!led_is_on)
                {
                    led_is_on = true;
                    set_led_state(true); // Turn on the LED if it's off
                }
                set_led_hsv(hue); // Call the new LED function
            }
            else
            {
                ESP_LOGW(TAG, "Missing 'hue' parameter in query string");
            }
        }
        free(buf);
    }
    else
    {
        ESP_LOGW(TAG, "No query string in /set_hue request");
    }

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"new_state\":%d}", led_is_on ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for setting LED brightness */
static esp_err_t set_brightness_get_handler(httpd_req_t *req)
{ // <<< NEW
    char *buf;
    size_t buf_len;
    uint8_t brightness = 0;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (!buf)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for query buffer");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal server error");
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[10];
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK)
            { // 'value' for brightness
                brightness = atoi(param);
                // Clamp brightness to 0-255 if needed, though slider limits usually handle this
                if (brightness > 255)
                    brightness = 255;

                ESP_LOGI(TAG, "Received brightness: %d", brightness);

                stop_led_effect(); // <<< NEW: Stop effect when setting solid color

                if (!led_is_on)
                { // If LED is off, turn it on
                    led_is_on = true;
                    set_led_state(true);
                }
                set_led_brightness(brightness); // Set brightness (hue will be last_set_hue)
            }
            else
            {
                ESP_LOGW(TAG, "Missing 'value' parameter in query string");
            }
        }
        free(buf);
    }
    else
    {
        ESP_LOGW(TAG, "No query string in /set_brightness request");
    }

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"new_state\":%d}", led_is_on ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for starting LED effects */
static esp_err_t start_effect_get_handler(httpd_req_t *req)
{ // <<< NEW HANDLER
    char *buf;
    size_t buf_len;
    led_effect_type_t effect_type = EFFECT_NONE;
    uint32_t speed_ms = 50; // Default speed
    uint16_t hue = 0;       // Default hue

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (!buf)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal server error");
            return ESP_FAIL;
        }

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param_type[10];
            char param_speed[10];
            char param_hue[10];

            if (httpd_query_key_value(buf, "type", param_type, sizeof(param_type)) == ESP_OK)
            {
                effect_type = atoi(param_type);
            }
            if (effect_type == EFFECT_NONE)
            {
                ESP_LOGI(TAG, "Effect type is NONE. Skipping effect start.");
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, "{\"status\":\"ignored\"}", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            }
            if (httpd_query_key_value(buf, "speed", param_speed, sizeof(param_speed)) == ESP_OK)
            {
                speed_ms = atoi(param_speed);
            }
            if (httpd_query_key_value(buf, "hue", param_hue, sizeof(param_hue)) == ESP_OK)
            {
                hue = atoi(param_hue);
            }
        }
        free(buf);
    }

    ESP_LOGI(TAG, "Received /start_effect: type=%d, speed=%lu, hue=%d", effect_type, speed_ms, hue);

    if (!led_is_on)
    { // Turn on LED if it's off
        led_is_on = true;
        set_led_state(true); // This will set it to the last solid color/brightness
    }
    start_led_effect(effect_type, speed_ms, hue);

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"effect_type\":%d}", effect_type);
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

    // New URI handler for setting hue
    httpd_uri_t set_hue_uri = {.uri = "/set_hue", .method = HTTP_GET, .handler = set_hue_get_handler, .user_ctx = NULL};
    httpd_register_uri_handler(server, &set_hue_uri);

    // New URI handler for setting brightness
    httpd_uri_t set_brightness_uri = {.uri = "/set_brightness", .method = HTTP_GET, .handler = set_brightness_get_handler, .user_ctx = NULL}; // <<< NEW
    httpd_register_uri_handler(server, &set_brightness_uri);

    // New URI handler for starting effects
    httpd_uri_t start_effect_uri = {.uri = "/start_effect", .method = HTTP_GET, .handler = start_effect_get_handler, .user_ctx = NULL}; // <<< NEW
    httpd_register_uri_handler(server, &start_effect_uri);

    return server;
}