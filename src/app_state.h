#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    MODE_CHARGE = 0,
    MODE_KEY_ACTIVE,
    MODE_IDLE
} app_mode_t;

typedef enum {
    EFFECT_STATIC = 0,
    EFFECT_BREATH,
    EFFECT_DRAGON_BREATH,
    EFFECT_CANDLE,
    EFFECT_FIRE_FLICKER,
    EFFECT_STROBE,
    EFFECT_PULSE,
    EFFECT_HEARTBEAT,
    EFFECT_FADE_IN_OUT,
    EFFECT_COUNT
} light_effect_t;

typedef struct {
    app_mode_t mode;
    bool usb_present;
    bool key_inserted;
    bool light_enabled;
    bool boost_enabled;
    uint8_t brightness;
    light_effect_t effect;
} app_state_snapshot_t;

esp_err_t app_state_init(void);
void app_state_get_snapshot(app_state_snapshot_t *out_snapshot);
bool app_state_update_inputs(bool usb_present, bool key_inserted, app_mode_t *previous_mode, app_mode_t *current_mode);
esp_err_t app_state_set_settings(int brightness, light_effect_t effect, bool has_brightness, bool has_effect);
void app_state_set_boost_enabled(bool boost_enabled);
const char *app_state_mode_to_string(app_mode_t mode);
const char *app_state_effect_to_string(light_effect_t effect);
bool app_state_effect_from_string(const char *value, light_effect_t *effect);
