#include "app_state.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static const char *TAG = "app_state";
static const char *NVS_NAMESPACE = "frieren";
static const char *NVS_KEY_BRIGHTNESS = "brightness";
static const char *NVS_KEY_EFFECT = "effect";

static SemaphoreHandle_t s_state_mutex;
static app_state_snapshot_t s_state;

static uint8_t clamp_brightness(int brightness)
{
    if (brightness < 0) {
        return 0;
    }

    if (brightness > 100) {
        return 100;
    }

    return (uint8_t)brightness;
}

static app_mode_t select_mode(bool usb_present, bool key_inserted)
{
    if (usb_present) {
        return MODE_CHARGE;
    }

    if (key_inserted) {
        return MODE_KEY_ACTIVE;
    }

    return MODE_IDLE;
}

static void recompute_locked(void)
{
    s_state.mode = select_mode(s_state.usb_present, s_state.key_inserted);
    s_state.light_enabled = (s_state.mode == MODE_KEY_ACTIVE) && (s_state.brightness > 0);
}

static esp_err_t save_settings_locked(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_BRIGHTNESS, s_state.brightness);
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, NVS_KEY_EFFECT, (int32_t)s_state.effect);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save settings: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

static void load_settings(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for load: %s", esp_err_to_name(err));
        return;
    }

    uint8_t brightness = 0;
    if (nvs_get_u8(handle, NVS_KEY_BRIGHTNESS, &brightness) == ESP_OK) {
        s_state.brightness = clamp_brightness(brightness);
    }

    int32_t effect = EFFECT_STATIC;
    if (nvs_get_i32(handle, NVS_KEY_EFFECT, &effect) == ESP_OK && effect >= 0 && effect < EFFECT_COUNT) {
        s_state.effect = (light_effect_t)effect;
    }

    nvs_close(handle);
}

esp_err_t app_state_init(void)
{
    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = MODE_IDLE;
    s_state.brightness = 70;
    s_state.effect = EFFECT_STATIC;
    s_state.boost_enabled = false;

    load_settings();

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        recompute_locked();
        xSemaphoreGive(s_state_mutex);
    }

    return ESP_OK;
}

void app_state_get_snapshot(app_state_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL || s_state_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        *out_snapshot = s_state;
        xSemaphoreGive(s_state_mutex);
    }
}

bool app_state_update_inputs(bool usb_present, bool key_inserted, app_mode_t *previous_mode, app_mode_t *current_mode)
{
    if (s_state_mutex == NULL) {
        return false;
    }

    bool changed = false;

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        app_mode_t old_mode = s_state.mode;

        s_state.usb_present = usb_present;
        s_state.key_inserted = key_inserted;
        recompute_locked();

        changed = old_mode != s_state.mode;

        if (previous_mode != NULL) {
            *previous_mode = old_mode;
        }
        if (current_mode != NULL) {
            *current_mode = s_state.mode;
        }

        xSemaphoreGive(s_state_mutex);
    }

    return changed;
}

esp_err_t app_state_set_settings(int brightness, light_effect_t effect, bool has_brightness, bool has_effect)
{
    if (s_state_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t save_err = ESP_OK;

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        if (has_brightness) {
            s_state.brightness = clamp_brightness(brightness);
        }

        if (has_effect && effect >= 0 && effect < EFFECT_COUNT) {
            s_state.effect = effect;
        }

        recompute_locked();
        save_err = save_settings_locked();

        xSemaphoreGive(s_state_mutex);
    }

    return save_err;
}

void app_state_set_boost_enabled(bool boost_enabled)
{
    if (s_state_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        s_state.boost_enabled = boost_enabled;
        xSemaphoreGive(s_state_mutex);
    }
}

const char *app_state_mode_to_string(app_mode_t mode)
{
    switch (mode) {
        case MODE_CHARGE:
            return "CHARGE";
        case MODE_KEY_ACTIVE:
            return "KEY_ACTIVE";
        case MODE_IDLE:
        default:
            return "IDLE";
    }
}

const char *app_state_effect_to_string(light_effect_t effect)
{
    switch (effect) {
        case EFFECT_STATIC:
            return "static";
        case EFFECT_BREATH:
            return "breath";
        case EFFECT_DRAGON_BREATH:
            return "dragon_breath";
        case EFFECT_CANDLE:
            return "candle";
        case EFFECT_FIRE_FLICKER:
            return "fire_flicker";
        case EFFECT_STROBE:
            return "strobe";
        case EFFECT_PULSE:
            return "pulse";
        case EFFECT_HEARTBEAT:
            return "heartbeat";
        case EFFECT_FADE_IN_OUT:
            return "fade_in_out";
        case EFFECT_COUNT:
        default:
            return "static";
    }
}

bool app_state_effect_from_string(const char *value, light_effect_t *effect)
{
    if (value == NULL || effect == NULL) {
        return false;
    }

    for (int i = 0; i < EFFECT_COUNT; ++i) {
        if (strcmp(value, app_state_effect_to_string((light_effect_t)i)) == 0) {
            *effect = (light_effect_t)i;
            return true;
        }
    }

    return false;
}
