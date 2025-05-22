#ifndef MAIN_WIFI_AP_H
#define MAIN_WIFI_AP_H

#include "esp_err.h" // Include this if your functions return esp_err_t, good practice
#include "esp_event.h" // For esp_event_base_t, etc.

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Wi-Fi in SoftAP mode.
 *
 * This function sets up the ESP32 as a Wi-Fi Access Point with the
 * predefined SSID, password, and channel. It registers an event handler
 * for Wi-Fi events.
 */
void wifi_init_softap(void);

// If you had other public functions in wifi.c, you'd declare them here too.
// For example:
// esp_err_t wifi_start_ap(void);
// esp_err_t wifi_stop_ap(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_WIFI_AP_H