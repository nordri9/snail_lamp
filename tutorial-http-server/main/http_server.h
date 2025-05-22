#ifndef MAIN_HTTP_SERVER_H
#define MAIN_HTTP_SERVER_H

// Include necessary headers for types used in the prototype
#include "esp_http_server.h" // For httpd_handle_t and related HTTP server types
#include "esp_err.h"         // For esp_err_t and ESP_OK, etc.

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts the HTTP web server and registers URI handlers.
 *
 * This function initializes and starts the ESP32's HTTP server.
 * It registers the default "/" URI handler for the "Hello World" page.
 *
 * @return The handle to the started HTTP server, or NULL if an error occurred during startup.
 */
httpd_handle_t start_webserver(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_HTTP_SERVER_H