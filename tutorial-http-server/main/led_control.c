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

// Set to 1 to use DMA for driving the LED strip, 0 otherwise
// Please note the RMT DMA feature is only available on chips e.g. ESP32-S3/P4
#define LED_STRIP_USE_DMA 0

#if LED_STRIP_USE_DMA
#define LED_STRIP_LED_COUNT 256
#define LED_STRIP_MEMORY_BLOCK_WORDS 1024
#else
#define LED_STRIP_LED_COUNT 2 // Still 2 LEDs for this example (can be 1 if you only have one physical LED)
#define LED_STRIP_MEMORY_BLOCK_WORDS 0
#endif

// GPIO assignment
#define LED_STRIP_GPIO_PIN 27 // Ensure this matches your physical connection

// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static const char *TAG = "led_control";

// Global handle for the LED strip so other functions can access it
static led_strip_handle_t global_led_strip_handle = NULL;

/**
 * @brief Configures the LED strip (WS2812B or similar) and stores its handle.
 * This function should be called once at application startup.
 * @return The handle to the configured LED strip.
 */
led_strip_handle_t configure_led(void)
{
    if (global_led_strip_handle != NULL) {
        ESP_LOGW(TAG, "LED strip already configured.");
        return global_led_strip_handle;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS,
        .flags.with_dma = LED_STRIP_USE_DMA,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &global_led_strip_handle));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");

    // Clear the strip initially to ensure all LEDs are off
    ESP_ERROR_CHECK(led_strip_clear(global_led_strip_handle));

    return global_led_strip_handle;
}

/**
 * @brief Sets the state of the first LED on the strip (on/off).
 * @param on True to turn the LED on (red color), false to turn it off.
 */
void set_led_state(bool on) {
    if (global_led_strip_handle == NULL) {
        ESP_LOGE(TAG, "LED strip not configured. Call configure_led() first.");
        return;
    }

    if (on) {
        // Set the first LED to red
        ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip_handle, 0, 10, 0, 0)); // LED 0, R=10, G=0, B=0
        ESP_ERROR_CHECK(led_strip_refresh(global_led_strip_handle));
        ESP_LOGI(TAG, "LED is ON (Red)");
    } else {
        // Clear the entire strip (turns all LEDs off)
        ESP_ERROR_CHECK(led_strip_clear(global_led_strip_handle));
        ESP_LOGI(TAG, "LED is OFF");
    }
}

// The led_test function is removed as its infinite loop would block the web server.
// The functionality of toggling is now handled by the web server through set_led_state.