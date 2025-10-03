#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // For mutex
#include "esp_log.h"
#include "esp_random.h"

#include "led_control.h"
#include "led_effects.h"

static const char *TAG = "LED_effects";

// --- State Variables ---
static TaskHandle_t s_animator_task_handle = NULL;
static SemaphoreHandle_t s_state_mutex = NULL;

// Use a single struct to hold all state, protected by the mutex
static struct
{
    led_effect_type_t type;
    uint32_t speed_ms;
    uint16_t color_hue;
    // Effect-specific state variables
    uint32_t state1; // e.g., hue for rainbow, offset for chase
    uint32_t state2; // e.g., position for wave
} s_effect_state = {.type = EFFECT_NONE, .speed_ms = 50};

// --- Animator Task ---

static void led_animator_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LED Animator Task Started");

    while (1)
    {
        // Lock the state while we read it
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);

        // Copy state locally to minimize mutex lock time
        led_effect_type_t current_type = s_effect_state.type;
        uint32_t speed_ms = s_effect_state.speed_ms;
        uint16_t hue = s_effect_state.color_hue;

        // Decide which effect to render for this frame
        switch (current_type)
        {
        case EFFECT_RAINBOW_CYCLE:
        {
            uint16_t current_hue = s_effect_state.state1;
            for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
            {
                set_pixel_hsv(i, (current_hue + (i * (360 / LED_STRIP_LED_COUNT))) % 360, 255, get_last_set_brightness());
            }
            s_effect_state.state1 = (current_hue + 1) % 360; // Update state for next frame
            break;
        }

        case EFFECT_THEATER_CHASE:
        {
            uint8_t r, g, b;
            hsv2rgb(hue, 255, get_last_set_brightness(), &r, &g, &b);
            led_control_clear_all();
            int offset = s_effect_state.state1;
            for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
            {
                if ((i + offset) % 3 == 0)
                {
                    led_strip_set_pixel(global_led_strip, i, r, g, b);
                }
            }
            s_effect_state.state1 = (offset + 1) % 3;
            break;
        }

        case EFFECT_SPARKLE:
        {
            // A better sparkle: set a random pixel, then clear it next frame
            // This avoids clearing the whole strip which causes flicker
            int last_pixel = s_effect_state.state1;
            led_strip_set_pixel(global_led_strip,last_pixel, 0, 0, 0); // Clear last sparkle

            int pixel_idx = esp_random() % LED_STRIP_LED_COUNT;
            uint8_t r, g, b;
            hsv2rgb(hue, 255, 200, &r, &g, &b); // Use a bright sparkle color
            led_strip_set_pixel(global_led_strip,pixel_idx, r, g, b);

            s_effect_state.state1 = pixel_idx;
            break;
        }

        case EFFECT_WAVE:
        {
            uint8_t r, g, b;
            hsv2rgb(hue, 255, get_last_set_brightness(), &r, &g, &b);
            led_control_clear_all();
            int wave_size = 5;
            int current_pos = s_effect_state.state1;
            for (int i = 0; i < wave_size; i++)
            {
                int pixel_idx = (current_pos + i) % LED_STRIP_LED_COUNT;
                led_strip_set_pixel(global_led_strip, pixel_idx, r, g, b);
            }
            s_effect_state.state1 = (current_pos + 1) % LED_STRIP_LED_COUNT;
            break;
        }

        case EFFECT_NONE:
        default:
            // Do nothing, the loop will just delay.
            // A `stop_led_effect` call already cleared the strip.
            break;
        }

        // Unlock the state
        xSemaphoreGive(s_state_mutex);

        // If an effect is active, refresh the strip
        if (current_type != EFFECT_NONE)
        {
            led_control_refresh_all();
        }

        // Delay until the next frame
        vTaskDelay(pdMS_TO_TICKS(speed_ms > 0 ? speed_ms : 50));
    }
}

// --- Public Control Functions ---

void start_led_effect(led_effect_type_t effect_type, uint32_t speed_ms, uint16_t color_hue)
{
    // Create the animator task if it doesn't exist
    if (s_animator_task_handle == NULL)
    {
        s_state_mutex = xSemaphoreCreateMutex();
        xTaskCreate(led_animator_task, "Animator", 4096, NULL, 5, &s_animator_task_handle);
    }

    // Lock the state for writing
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);

    // If we are starting a new effect, clear the strip and reset state
    if (s_effect_state.type != effect_type)
    {
        led_control_clear_all();
        led_control_refresh_all();
        s_effect_state.state1 = 0;
        s_effect_state.state2 = 0;
    }

    // Update the shared state struct
    s_effect_state.type = effect_type;
    s_effect_state.speed_ms = speed_ms;
    s_effect_state.color_hue = color_hue;

    ESP_LOGI(TAG, "Set effect to %d with speed %lu", effect_type, speed_ms);

    // Unlock
    xSemaphoreGive(s_state_mutex);
}

void stop_led_effect(void)
{
    // Just set the effect type to NONE. The animator task will handle the rest.
    start_led_effect(EFFECT_NONE, 0, 0);
}