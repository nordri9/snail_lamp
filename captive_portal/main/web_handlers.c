// File: main/web_handlers.c

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"

// Include the headers for your LED control functions
#include "led_control.h"
#include "led_effects.h"
#include "web_handlers.h"

static const char *TAG = "web_handlers";

// --- State Variable (Moved from webserver.c) ---
// Keep track of the LED state, now local to this file
static bool led_is_on = true;

// --- Handler Functions (Moved from webserver.c) ---

/* Handler for "/status" URI */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received /status request");

    uint16_t current_hue = get_last_set_hue();
    uint8_t current_brightness = get_last_set_brightness();

    char resp_str[150];
    sprintf(resp_str, "{\"led_state\":%d, \"current_hue\":%d, \"current_brightness\":%d}",
            led_is_on ? 1 : 0, current_hue, current_brightness);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for "/toggle" URI */
static esp_err_t toggle_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received /toggle request");
    stop_led_effect();
    led_is_on = !led_is_on;
    set_led_state(led_is_on);

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"new_state\":%d}", led_is_on ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for setting LED color via hue */
static esp_err_t set_hue_get_handler(httpd_req_t *req)
{
    char buf[64]; // Use stack buffer for query string
    uint16_t hue = 0;

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
    {
        char param[10];
        if (httpd_query_key_value(buf, "hue", param, sizeof(param)) == ESP_OK)
        {
            hue = atoi(param);
            ESP_LOGI(TAG, "Received hue: %d", hue);
            stop_led_effect();
            if (!led_is_on)
            {
                led_is_on = true;
                set_led_state(true);
            }
            set_led_hsv(hue);
        }
    }

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"new_state\":%d}", led_is_on ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for setting LED brightness */
static esp_err_t set_brightness_get_handler(httpd_req_t *req)
{
    char buf[64];
    uint8_t brightness = 0;

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
    {
        char param[10];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK)
        {
            brightness = atoi(param);
            if (brightness > 255)
                brightness = 255;

            ESP_LOGI(TAG, "Received brightness: %d", brightness);
            stop_led_effect();
            if (!led_is_on)
            {
                led_is_on = true;
                set_led_state(true);
            }
            set_led_brightness(brightness);
        }
    }

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"new_state\":%d}", led_is_on ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for starting LED effects */
static esp_err_t start_effect_get_handler(httpd_req_t *req)
{
    char buf[128];
    led_effect_type_t effect_type = EFFECT_NONE;
    uint32_t speed_ms = 50;
    uint16_t hue = 0;

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
    {
        char param[10];
        if (httpd_query_key_value(buf, "type", param, sizeof(param)) == ESP_OK)
        {
            effect_type = atoi(param);
        }
        if (effect_type == EFFECT_NONE)
        {
            stop_led_effect();
        }
        else
        {
            if (httpd_query_key_value(buf, "speed", param, sizeof(param)) == ESP_OK)
            {
                speed_ms = atoi(param);
            }
            if (httpd_query_key_value(buf, "hue", param, sizeof(param)) == ESP_OK)
            {
                hue = atoi(param);
            }
            if (!led_is_on)
            {
                led_is_on = true;
                set_led_state(true);
            }
            start_led_effect(effect_type, speed_ms, hue);
        }
    }

    char resp_str[50];
    sprintf(resp_str, "{\"status\":\"success\", \"effect_type\":%d}", effect_type);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* Handler for "/set_effect_speed" URI
   Query param: speed (ms)
   If an effect is currently active, update its speed in-flight.
   If no effect active, return an error JSON.
*/
static esp_err_t set_effect_speed_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received /set_effect_speed request");

    char buf[64];
    uint32_t speed_ms = 0;
    bool speed_provided = false;

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
    {
        char param[16];
        if (httpd_query_key_value(buf, "speed", param, sizeof(param)) == ESP_OK)
        {
            speed_ms = (uint32_t)atoi(param);
            speed_provided = true;
        }
    }

    if (!speed_provided)
    {
        const char *msg = "{\"status\":\"error\",\"message\":\"missing 'speed' parameter\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, msg, strlen(msg));
        return ESP_OK;
    }

    led_effect_type_t cur = get_current_effect_type();
    if (cur == EFFECT_NONE)
    {
        // nothing running
        char resp_str[128];
        snprintf(resp_str, sizeof(resp_str),
                 "{\"status\":\"error\",\"message\":\"no active effect to update\",\"requested_speed\":%lu}",
                 speed_ms);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }

    // Update speed in-flight
    update_effect_speed(speed_ms);

    char resp_str[128];
    snprintf(resp_str, sizeof(resp_str),
             "{\"status\":\"success\",\"effect_type\":%d,\"new_speed\":%lu}",
             (int)cur, speed_ms);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

// --- URI Registration Function ---

esp_err_t register_api_uri_handlers(httpd_handle_t server)
{
    httpd_uri_t status_uri = {
        .uri = "/status", .method = HTTP_GET, .handler = status_get_handler};
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t toggle_uri = {
        .uri = "/toggle", .method = HTTP_GET, .handler = toggle_get_handler};
    httpd_register_uri_handler(server, &toggle_uri);

    httpd_uri_t set_hue_uri = {
        .uri = "/set_hue", .method = HTTP_GET, .handler = set_hue_get_handler};
    httpd_register_uri_handler(server, &set_hue_uri);

    httpd_uri_t set_brightness_uri = {
        .uri = "/set_brightness", .method = HTTP_GET, .handler = set_brightness_get_handler};
    httpd_register_uri_handler(server, &set_brightness_uri);

    httpd_uri_t start_effect_uri = {
        .uri = "/start_effect", .method = HTTP_GET, .handler = start_effect_get_handler};
    httpd_register_uri_handler(server, &start_effect_uri);

    httpd_uri_t set_effect_speed_uri = {
        .uri = "/set_effect_speed", .method = HTTP_GET, .handler = set_effect_speed_get_handler};
    httpd_register_uri_handler(server, &set_effect_speed_uri);

    return ESP_OK;
}