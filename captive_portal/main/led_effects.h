#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include <stdint.h>
#include "led_control.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Enum for different LED effects
    typedef enum
    {
        EFFECT_NONE = 0,
        EFFECT_RAINBOW_CYCLE,
        EFFECT_THEATER_CHASE,
        EFFECT_SPARKLE,
        EFFECT_WAVE, // A simple wave effect
        EFFECT_COUNT // Keep this last for counting effects
    } led_effect_type_t;

    /**
     * @brief Starts a specified LED effect.
     * If an effect is already running, it will be stopped first.
     * @param effect_type The type of effect to start.
     * @param speed_ms The delay in milliseconds between animation frames/steps.
     * Interpretation may vary per effect (e.g., cycle speed, chase speed).
     * @param color_hue (Optional) A base hue for some effects, e.g., for sparkle.
     * Pass 0 if not applicable.
     */
    void start_led_effect(led_effect_type_t effect_type, uint32_t speed_ms, uint16_t color_hue);

    /**
     * @brief Stops any currently running LED effect and clears the strip.
     */
    void stop_led_effect(void);

#ifdef __cplusplus
}
#endif

#endif // LED_EFFECTS_H