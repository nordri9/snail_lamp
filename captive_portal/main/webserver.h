#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts the HTTP web server.
 *
 * This function initializes and starts an HTTP server instance. It configures
 * the server with default settings, enables LRU purging, and registers the
 * root URI handler as well as a 404 error handler that redirects all unknown
 * requests to the root.
 *
 * @return A handle to the started HTTP server, or NULL if the server failed to start.
 */
httpd_handle_t start_webserver(void);

#ifdef __cplusplus
}
#endif

#endif // WEBSERVER_H