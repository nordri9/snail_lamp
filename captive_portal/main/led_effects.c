#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h" // For esp_random() for sparkle effect

#include "led_control.h" // Includes led_strip.h and exposes global_led_strip, LED_STRIP_LED_COUNT, set_pixel_hsv etc.
#include "led_effects.h"

static const char *TAG = "LED_effects";

// Task handle for the currently running effect
static TaskHandle_t s_effect_task_handle = NULL;
// Current effect type (for status/debugging)
static led_effect_type_t s_current_effect_type = EFFECT_NONE;
// Parameters for the current effect task
static volatile bool s_effect_task_should_stop = false;

typedef struct
{
    uint32_t speed_ms;
    uint16_t color_hue;
} effect_params_t;

// --- Internal Effect Task Functions ---

// Rainbow Cycle Effect Task
static void rainbow_cycle_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Rainbow Cycle effect");
    effect_params_t *params = (effect_params_t *)pvParameters;
    uint32_t speed_ms = params->speed_ms;

    uint16_t hue = 0;
    while (!s_effect_task_should_stop)
    {
        for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
        {
            set_pixel_hsv(i, (hue + (i * (360 / LED_STRIP_LED_COUNT))) % 360, 255, get_last_set_brightness());
        }
        led_control_refresh_all();
        hue = (hue + 1) % 360;
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
    }

    // Clean up
    ESP_LOGI(TAG, "Rainbow task exiting");
    s_effect_task_should_stop = false;
    vTaskDelete(NULL);
}

// Theater Chase Effect Task
static void theater_chase_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Theater Chase effect");
    effect_params_t *params = (effect_params_t *)pvParameters;
    uint32_t speed_ms = params->speed_ms;
    uint16_t hue = params->color_hue; // Use passed hue for the chase color

    uint8_t r, g, b;
    hsv2rgb(hue, 255, get_last_set_brightness(), &r, &g, &b); // Get RGB of chase color

    int offset = 0;
    while (true)
    {
        led_control_clear_all(); // Clear before drawing

        for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
        {
            if ((i + offset) % 3 == 0)
            { // Light up every 3rd LED (offset by `offset`)
                ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip, i, r, g, b));
            }
        }
        led_control_refresh_all();
        offset = (offset + 1) % 3; // Shift the pattern
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
    }
    vTaskDelete(NULL);
}

// Sparkle Effect Task
static void sparkle_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Sparkle effect");
    effect_params_t *params = (effect_params_t *)pvParameters;
    uint32_t speed_ms = params->speed_ms; // Delay between sparkles
    uint16_t hue = params->color_hue;     // Base hue for sparkles (or can be random)

    uint8_t r, g, b;
    uint8_t sparkle_brightness = 150; // Sparkle brightness

    while (true)
    {
        led_control_clear_all(); // Start with a clear strip

        // Set one or more random pixels
        int pixel_idx = esp_random() % LED_STRIP_LED_COUNT;
        hsv2rgb(hue, 255, sparkle_brightness, &r, &g, &b); // Use fixed saturation for sparkle
        ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip, pixel_idx, r, g, b));

        led_control_refresh_all();
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // Optional: fade out
        led_control_clear_all();
        led_control_refresh_all();
        vTaskDelay(pdMS_TO_TICKS(speed_ms / 2)); // Shorter delay for "fade out"
    }
    vTaskDelete(NULL);
}

// Simple Wave Effect Task (traveling solid block of color)
static void wave_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Wave effect");
    effect_params_t *params = (effect_params_t *)pvParameters;
    uint32_t speed_ms = params->speed_ms;
    uint16_t hue = params->color_hue; // Wave color hue

    uint8_t r, g, b;
    hsv2rgb(hue, 255, get_last_set_brightness(), &r, &g, &b);

    int wave_size = 5; // Size of the wave block
    int current_pos = 0;

    while (true)
    {
        led_control_clear_all(); // Clear before drawing

        for (int i = 0; i < wave_size; i++)
        {
            int pixel_idx = (current_pos + i) % LED_STRIP_LED_COUNT;
            ESP_ERROR_CHECK(led_strip_set_pixel(global_led_strip, pixel_idx, r, g, b));
        }
        led_control_refresh_all();

        current_pos = (current_pos + 1) % LED_STRIP_LED_COUNT; // Move the wave
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
    }
    vTaskDelete(NULL);
}

// --- Public Effect Control Functions ---

void start_led_effect(led_effect_type_t effect_type, uint32_t speed_ms, uint16_t color_hue)
{
    // Stop any existing effect first
    stop_led_effect();



    if (effect_type == EFFECT_NONE)
    {
        ESP_LOGI(TAG, "Stopping effects (EFFECT_NONE requested)");
        // If EFFECT_NONE, we just stopped the previous one, so nothing more to do.
        return;
    }

    s_current_effect_type = effect_type;
    effect_params_t *params = (effect_params_t *)malloc(sizeof(effect_params_t));
    if (params == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for effect parameters!");
        s_current_effect_type = EFFECT_NONE;
        return;
    }
    params->speed_ms = speed_ms;
    params->color_hue = color_hue;

    BaseType_t xReturned;
    switch (effect_type)
    {
    case EFFECT_RAINBOW_CYCLE:
        xReturned = xTaskCreate(rainbow_cycle_task, "RainbowCycle", 4096, params, 5, &s_effect_task_handle);
        break;
    case EFFECT_THEATER_CHASE:
        xReturned = xTaskCreate(theater_chase_task, "TheaterChase", 4096, params, 5, &s_effect_task_handle);
        break;
    case EFFECT_SPARKLE:
        xReturned = xTaskCreate(sparkle_task, "Sparkle", 4096, params, 5, &s_effect_task_handle);
        break;
    case EFFECT_WAVE:
        xReturned = xTaskCreate(wave_task, "Wave", 4096, params, 5, &s_effect_task_handle);
        break;
    default:
        ESP_LOGW(TAG, "Unknown effect type: %d", effect_type);
        s_current_effect_type = EFFECT_NONE;
        free(params);
        return;
    }

    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create effect task!");
        s_current_effect_type = EFFECT_NONE;
        free(params);
        return;
    }
    ESP_LOGI(TAG, "Started new effect task: %d", effect_type);
}

void stop_led_effect(void)
{
    if (s_effect_task_handle != NULL)
    {
        ESP_LOGI(TAG, "Requesting current LED effect task to stop");
        s_effect_task_should_stop = true;

        // Wait for task to exit itself
        while (eTaskGetState(s_effect_task_handle) != eDeleted)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        s_effect_task_handle = NULL;
        s_current_effect_type = EFFECT_NONE;

        led_control_clear_all();
        led_control_refresh_all();
    }
}