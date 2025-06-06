#ifndef MAIN_LED_CONTROL_H
#define MAIN_LED_CONTROL_H

#include "esp_err.h" // Include this if your functions return esp_err_t, good practice
#include "esp_event.h" // For esp_event_base_t, etc.
#include "led_strip.h"

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

void set_led_hsv(uint16_t hue);
void set_led_brightness(uint8_t brightness);
uint16_t get_last_set_hue(void);
uint8_t get_last_set_brightness(void);

// Expose the LED strip handle and count so effects can directly manipulate pixels
extern led_strip_handle_t global_led_strip; // Expose the global handle
extern const int LED_STRIP_LED_COUNT;       // Expose the LED count

// Helper function to set a single pixel's color using HSV
void set_pixel_hsv(uint16_t pixel_idx, uint16_t h, uint8_t s, uint8_t v);

// Wrappers for led_strip functions for external use
void led_control_clear_all(void);
void led_control_refresh_all(void);

// Internal HSV to RGB conversion (made public for effects)
void hsv2rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b);


#ifdef __cplusplus
}
#endif

#endif // MAIN_LED_CONTROL_H