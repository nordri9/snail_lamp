#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "led_control.h"
#include "dns_server.h"

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

static int slider_pwm_value = 0;

static esp_err_t slider_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received /slider request");
    char *buf;
    size_t buf_len;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Query string: %s", buf);
            char param[32];
            /* Get value of expected key `value` */
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK)
            {
                slider_pwm_value = atoi(param);
                if (slider_pwm_value < 0)
                    slider_pwm_value = 0;
                if (slider_pwm_value > 255)
                    slider_pwm_value = 255;
                ESP_LOGI(TAG, "Slider value: %d", slider_pwm_value);

                // Set PWM duty cycle
                set_overall_brightness(slider_pwm_value);
            }
        }
        free(buf);
    }

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"received_value\":%d}", slider_pwm_value);
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

// Handler for all other requests - redirects to captive portal
static esp_err_t captive_portal_redirect_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Redirecting to captive portal: %s", req->uri);

    // Respond to connectivity checks
    if (strcmp(req->uri, "/generate_204") == 0 ||
        strcmp(req->uri, "/hotspot-detect.html") == 0 ||
        strcmp(req->uri, "/ncsi.txt") == 0)
    {
        ESP_LOGI(TAG, "Responding to connectivity check with 204 No Content");
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // Otherwise, redirect to the captive portal HTML
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/data/index.html"); // or /spiffs/index.html if that's your portal page
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Handler for when the user clicks 'Agree' or similar
static esp_err_t portal_release_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Client released from captive portal");
    // Here you would implement logic to disable DNS redirection for this client's IP
    // This is more complex and usually involves tracking client IPs in the DNS server.
    // For a simple demo, you might just redirect them to a success page.

    httpd_resp_sendstr(req, "You are now connected!");
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

    httpd_uri_t slider_uri = {
        .uri = "/slider",
        .method = HTTP_GET,
        .handler = slider_get_handler, // Function to handle slider changes
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &slider_uri);

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

    // New URI handler for setting hue
    httpd_uri_t set_hue_uri = {.uri = "/set_hue", .method = HTTP_GET, .handler = set_hue_get_handler, .user_ctx = NULL};
    httpd_register_uri_handler(server, &set_hue_uri);

    // Handler for releasing from portal
    httpd_uri_t release_uri = {
        .uri = "/release_portal", // Or whatever URL your portal button points to
        .method = HTTP_GET,
        .handler = portal_release_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &release_uri);

    // IMPORTANT: This should be registered LAST as a wildcard handler
    // It will catch all requests not handled by other specific URIs.
    httpd_uri_t catch_all_uri = {
        .uri = "/*", // Wildcard to catch all
        .method = HTTP_GET,
        .handler = captive_portal_redirect_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &catch_all_uri);

    return server;
}
