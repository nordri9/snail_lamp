// File: main/web_handlers.h

#ifndef WEB_HANDLERS_H_
#define WEB_HANDLERS_H_

#include "esp_http_server.h"

/**
 * @brief Registers all API URI handlers for controlling the LED.
 *
 * This function sets up the application-specific endpoints for:
 * - /status
 * - /toggle
 * - /set_hue
 * - /set_brightness
 * - /start_effect
 *
 * @param server The handle to the HTTPD server instance.
 * @return esp_err_t ESP_OK on success, or an error code if registration fails.
 */
esp_err_t register_api_uri_handlers(httpd_handle_t server);

#endif // WEB_HANDLERS_H_