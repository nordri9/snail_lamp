/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

#include "led_control.h"

// Set to 1 to use DMA for driving the LED strip, 0 otherwise
// Please note the RMT DMA feature is only available on chips e.g. ESP32-S3/P4
#define LED_STRIP_USE_DMA 0

#if LED_STRIP_USE_DMA
// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 256
#define LED_STRIP_MEMORY_BLOCK_WORDS 1024 // this determines the DMA block size
#else
// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 50
#define LED_STRIP_MEMORY_BLOCK_WORDS 0 // let the driver choose a proper memory block size automatically
#endif                                 // LED_STRIP_USE_DMA

// GPIO assignment
#define LED_STRIP_GPIO_PIN 27

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static const char *TAG = "LED_control";

// Make led_strip handle global or pass it around
static led_strip_handle_t global_led_strip;

static uint16_t s_last_set_hue = 0;

led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,                        // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_COUNT,                             // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,                               // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip: GRB
        .flags = {
            .invert_out = false, // don't invert the output signal
        }};

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,                    // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ,             // RMT counter clock frequency
        .mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS, // the memory block size used by the RMT channel
        .flags = {
            .with_dma = LED_STRIP_USE_DMA, // Using DMA can improve performance when driving more LEDs
        }};

    // LED Strip object handle
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &global_led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return global_led_strip;
}

void set_led_state(bool on)
{
    if (global_led_strip == NULL)
    {
        ESP_LOGE(TAG, "LED strip not configured. Call configure_led() first.");
        return;
    }

    if (on)
    {
        // When turning ON, default to red (Hue 0)
        set_led_hsv(50);
        ESP_LOGI(TAG, "LED is ON (default Red)");
    }
    else
    {
        ESP_ERROR_CHECK(led_strip_clear(global_led_strip));
        ESP_LOGI(TAG, "LED is OFF");
    }
}

// Optional: function to get current LED state (if needed for UI update)
bool get_led_state(void)
{
    // This is a simple state tracking. In a more complex app,
    // you might read actual GPIO state if directly controlling GPIO.
    // For WS2812, we rely on the state we're tracking.
    // For this example, we'll track it in http_server.c as `led_is_on`
    return false;
} // Will be tracked in http_server.c

/**
 * @brief Sets the overall brightness of the LED strip.
 *
 * This function iterates through all LEDs in the strip and sets their color
 * based on a scaled version of a base color (currently a shade of blue: R=0, G=50, B=255).
 * The brightness level is applied uniformly across all pixels.
 *
 * @param brightness A value from 0 (off) to 255 (full brightness).
 * A value of 0 will turn off all LEDs.
 */
void set_overall_brightness(uint8_t brightness)
{
    if (!global_led_strip)
    {
        ESP_LOGE(TAG, "LED strip not configured! Call configure_led() first.");
        return;
    }

    // Define a base color that will be scaled by the brightness.
    // Using the same blue from set_led_state as a default base.
    uint8_t base_red = 0;
    uint8_t base_green = 50;
    uint8_t base_blue = 255;

    // Calculate the scaled RGB values based on the desired brightness.
    // The brightness value (0-255) acts as a percentage of the base color's intensity.
    // We cast to float for calculation precision and then back to uint8_t.
    uint8_t scaled_red = (uint8_t)((float)base_red * brightness / 255.0);
    uint8_t scaled_green = (uint8_t)((float)base_green * brightness / 255.0);
    uint8_t scaled_blue = (uint8_t)((float)base_blue * brightness / 255.0);

    // If brightness is 0, all scaled values will be 0, effectively turning off the LEDs.
    // Otherwise, set each pixel to the scaled color.
    for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
    {
        // Sets the pixel color. The `ESP_ERROR_CHECK` macro will abort on error
        // if the pixel setting fails (e.g., invalid handle or index).
        ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip, i, scaled_red, scaled_green, scaled_blue));
    }

    // After setting all pixels, refresh the LED strip to display the changes.
    // This sends the updated pixel data to the physical LED strip.
    ESP_ERROR_CHECK(led_strip_refresh(global_led_strip));

    ESP_LOGI(TAG, "LED strip brightness set to %d (scaled color R:%d G:%d B:%d)", brightness, scaled_red, scaled_green, scaled_blue);
}

/**
 * @brief Converts HSV (Hue, Saturation, Value) to RGB.
 * Saturation and Value are fixed to 100% for full color vibrancy.
 * @param h Hue (0-359 degrees)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
static void hsv2rgb(uint16_t h, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t s = 255; // Saturation (0-255)
    uint8_t v = 255; // Value/Brightness (0-255)

    if (s == 0)
    {
        *r = *g = *b = v; // Gray scale
        return;
    }

    uint16_t region = h / 60;
    uint16_t remainder = h % 60;

    uint8_t p = (v * (255 - s)) / 255;
    uint8_t q = (v * (255 - (s * remainder) / 60)) / 255;
    uint8_t t = (v * (255 - (s * (60 - remainder)) / 60)) / 255;

    switch (region)
    {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    case 5:
        *r = v;
        *g = p;
        *b = q;
        break;
    default:
        *r = v;
        *g = t;
        *b = p;
        break; // Should not happen
    }
}

/**
 * @brief Sets the LED color based on a hue value.
 * @param hue Hue value (0-359).
 */
void set_led_hsv(uint16_t hue)
{
    if (global_led_strip == NULL)
    {
        ESP_LOGE(TAG, "LED strip not configured. Call configure_led() first.");
        return;
    }

    uint8_t r, g, b;
    hsv2rgb(hue, &r, &g, &b);

    // Apply color to all LEDs in the strip (or just the first one if LED_STRIP_LED_COUNT is 1)
    for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
    {
        ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip, i, r, g, b));
    }
    ESP_ERROR_CHECK(led_strip_refresh(global_led_strip));
    ESP_LOGI(TAG, "LED color set to H:%d -> R:%d G:%d B:%d", hue, r, g, b);
}

uint16_t get_last_set_hue(void)
{
    return s_last_set_hue;
}