#ifndef MAIN_LED_CONTROL_H
#define MAIN_LED_CONTROL_H

#include "esp_err.h" // Include this if your functions return esp_err_t, good practice
#include "esp_event.h" // For esp_event_base_t, etc.

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configures the LED strip. Should be called once at startup.
 * @return Handle to the LED strip.
 */
led_strip_handle_t configure_led(void);

/**
 * @brief Sets the state of the LED(s).
 * @param on True to turn the LED on, false to turn it off.
 */
void set_led_state(bool on);

#ifdef __cplusplus
}
#endif

#endif // MAIN_LED_CONTROL_H