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

// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT_INTERNAL 44
#define LED_STRIP_MEMORY_BLOCK_WORDS 0 // let the driver choose a proper memory block size automatically
                                       // LED_STRIP_USE_DMA

// GPIO assignment
#define LED_STRIP_GPIO_PIN 27

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static const char *TAG = "LED_control";

// Make led_strip handle global or pass it around
led_strip_handle_t global_led_strip = NULL;
const int LED_STRIP_LED_COUNT = LED_STRIP_LED_COUNT_INTERNAL;

static uint16_t s_last_set_hue = 0;
static uint8_t s_last_set_brightness = 255;

led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,                        // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_COUNT_INTERNAL,                    // The number of LEDs in the strip,
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
            .with_dma = 0, // Using DMA can improve performance when driving more LEDs
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
        // When turning ON, use the last set hue
        set_led_hsv(s_last_set_hue); // <<< MODIFIED: Use stored hue
        ESP_LOGI(TAG, "LED is ON (reverting to H:%d)", s_last_set_hue);
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
    return false;
} // Will be tracked in http_server.c

void hsv2rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0)
    {
        *r = *g = *b = v; // Gray scale
        return;
    }

    uint16_t region = h / 60;
    uint16_t remainder = h % 60;

    float f_v = (float)v / 255.0f;
    float f_s = (float)s / 255.0f;

    float p = f_v * (1.0f - f_s);
    float q = f_v * (1.0f - (f_s * remainder) / 60.0f);
    float t = f_v * (1.0f - (f_s * (60.0f - remainder) / 60.0f));

    uint8_t temp_r, temp_g, temp_b;

    switch (region)
    {
    case 0:
        temp_r = (uint8_t)(f_v * 255.0f);
        temp_g = (uint8_t)(t * 255.0f);
        temp_b = (uint8_t)(p * 255.0f);
        break;
    case 1:
        temp_r = (uint8_t)(q * 255.0f);
        temp_g = (uint8_t)(f_v * 255.0f);
        temp_b = (uint8_t)(p * 255.0f);
        break;
    case 2:
        temp_r = (uint8_t)(p * 255.0f);
        temp_g = (uint8_t)(f_v * 255.0f);
        temp_b = (uint8_t)(t * 255.0f);
        break;
    case 3:
        temp_r = (uint8_t)(p * 255.0f);
        temp_g = (uint8_t)(q * 255.0f);
        temp_b = (uint8_t)(f_v * 255.0f);
        break;
    case 4:
        temp_r = (uint8_t)(t * 255.0f);
        temp_g = (uint8_t)(p * 255.0f);
        temp_b = (uint8_t)(f_v * 255.0f);
        break;
    case 5:
        temp_r = (uint8_t)(f_v * 255.0f);
        temp_g = (uint8_t)(p * 255.0f);
        temp_b = (uint8_t)(q * 255.0f);
        break;
    default:
        temp_r = (uint8_t)(f_v * 255.0f);
        temp_g = (uint8_t)(t * 255.0f);
        temp_b = (uint8_t)(p * 255.0f);
        break;
    }
    *r = temp_r;
    *g = temp_g;
    *b = temp_b;
}

static void set_led_hsv_internal(uint16_t hue, uint8_t brightness)
{
    if (global_led_strip == NULL)
    {
        ESP_LOGE(TAG, "LED strip not configured. Call configure_led() first.");
        return;
    }

    uint8_t r, g, b;
    // Here, hsv2rgb is called with:
    // h: the 'hue' parameter
    // s: 255 (full saturation for static colors)
    // v: the 'brightness' parameter
    hsv2rgb(hue, 255, brightness, &r, &g, &b);

    // This loop applies the calculated RGB color to ALL LEDs
    for (int i = 0; i < LED_STRIP_LED_COUNT_INTERNAL; i++) // Note: LED_STRIP_LED_COUNT is exposed for effects
    {
        ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip, i, r, g, b));
    }
    ESP_ERROR_CHECK(led_strip_refresh(global_led_strip));
    ESP_LOGI(TAG, "LED color set to H:%d S:%d V:%d -> R:%d G:%d B:%d", hue, 255, brightness, r, g, b);
}

void set_pixel_hsv(uint16_t pixel_idx, uint16_t h, uint8_t s, uint8_t v)
{
    if (global_led_strip == NULL)
    {
        ESP_LOGE(TAG, "LED strip not configured. Call configure_led() first.");
        return;
    }
    if (pixel_idx >= LED_STRIP_LED_COUNT_INTERNAL)
    {
        ESP_LOGE(TAG, "Pixel index %d out of bounds (max %d)", pixel_idx, LED_STRIP_LED_COUNT_INTERNAL - 1);
        return;
    }

    uint8_t r, g, b;

    hsv2rgb(h, s, v, &r, &g, &b); // Call the now more flexible hsv2rgb
    ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip, pixel_idx, r, g, b));
}

// Wrapper for led_strip_clear
void led_control_clear_all(void)
{
    if (global_led_strip != NULL)
    {
        ESP_ERROR_CHECK(led_strip_clear(global_led_strip));
    }
}

// Wrapper for led_strip_refresh
void led_control_refresh_all(void)
{
    if (global_led_strip != NULL)
    {
        ESP_ERROR_CHECK(led_strip_refresh(global_led_strip));
    }
}


void set_led_hsv(uint16_t hue)
{
    s_last_set_hue = hue % 360;                                  // Store the new hue
    set_led_hsv_internal(s_last_set_hue, s_last_set_brightness); // Use stored brightness
}

void set_led_brightness(uint8_t brightness)
{
    s_last_set_brightness = brightness;                          // Store the new brightness
    set_led_hsv_internal(s_last_set_hue, s_last_set_brightness); // Use stored hue
}

uint16_t get_last_set_hue(void)
{
    return s_last_set_hue;
}

uint8_t get_last_set_brightness(void)
{
    return s_last_set_brightness;
}