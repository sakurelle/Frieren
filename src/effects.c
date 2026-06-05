#include "effects.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"
#include "app_state.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "light_pwm.h"

static const char *TAG = "effects";
static const float TWO_PI = 6.28318530718f;

typedef struct {
    float candle_level;
    float candle_target;
    int64_t candle_next_us;
    float fire_level;
    float fire_target;
    int64_t fire_next_us;
    float dragon_flash_level;
    float dragon_flash_decay_per_second;
    int64_t dragon_next_flash_us;
} effect_context_t;

static float clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}

static float random_float(float min_value, float max_value)
{
    uint32_t value = esp_random();
    float normalized = (float)(value & 0xFFFF) / 65535.0f;
    return min_value + ((max_value - min_value) * normalized);
}

static void reset_context(effect_context_t *ctx, int64_t now_us)
{
    ctx->candle_level = 0.82f;
    ctx->candle_target = 0.86f;
    ctx->candle_next_us = now_us;

    ctx->fire_level = 0.72f;
    ctx->fire_target = 0.76f;
    ctx->fire_next_us = now_us;

    ctx->dragon_flash_level = 0.0f;
    ctx->dragon_flash_decay_per_second = 0.0f;
    ctx->dragon_next_flash_us = now_us + 600000;
}

static float step_towards(float current, float target, float amount)
{
    if (amount >= 1.0f) {
        return target;
    }

    return current + ((target - current) * amount);
}

static float pulse_window(float elapsed_ms, float start_ms, float attack_ms, float release_ms)
{
    if (elapsed_ms < start_ms) {
        return 0.0f;
    }

    float local = elapsed_ms - start_ms;
    if (local < attack_ms) {
        return attack_ms > 0.0f ? (local / attack_ms) : 1.0f;
    }

    local -= attack_ms;
    if (local < release_ms) {
        return release_ms > 0.0f ? (1.0f - (local / release_ms)) : 0.0f;
    }

    return 0.0f;
}

static float compute_effect(light_effect_t effect, effect_context_t *ctx, int64_t now_us, float dt_seconds)
{
    float time_seconds = (float)now_us / 1000000.0f;
    float value = 0.0f;

    switch (effect) {
        case EFFECT_STATIC:
            value = 1.0f;
            break;

        case EFFECT_BREATH: {
            float cycle = 0.5f - (0.5f * cosf(TWO_PI * time_seconds / 3.2f));
            value = 0.08f + (0.92f * cycle);
            break;
        }

        case EFFECT_DRAGON_BREATH: {
            float cycle = 0.5f - (0.5f * cosf(TWO_PI * time_seconds / 4.6f));
            float base = 0.15f + (0.70f * cycle);

            if (now_us >= ctx->dragon_next_flash_us) {
                ctx->dragon_flash_level = random_float(0.18f, 0.45f);
                ctx->dragon_flash_decay_per_second = random_float(1.6f, 3.4f);
                ctx->dragon_next_flash_us = now_us + (int64_t)(random_float(450.0f, 1400.0f) * 1000.0f);
            }

            if (ctx->dragon_flash_level > 0.0f) {
                ctx->dragon_flash_level -= ctx->dragon_flash_decay_per_second * dt_seconds;
                if (ctx->dragon_flash_level < 0.0f) {
                    ctx->dragon_flash_level = 0.0f;
                }
            }

            value = base + ctx->dragon_flash_level;
            break;
        }

        case EFFECT_CANDLE: {
            if (now_us >= ctx->candle_next_us) {
                ctx->candle_target = random_float(0.60f, 1.00f);
                ctx->candle_next_us = now_us + (int64_t)(random_float(90.0f, 220.0f) * 1000.0f);
            }

            ctx->candle_level = step_towards(ctx->candle_level, ctx->candle_target, clamp01(dt_seconds * 4.0f));
            value = ctx->candle_level + (0.03f * sinf(TWO_PI * time_seconds * 2.2f));
            break;
        }

        case EFFECT_FIRE_FLICKER: {
            if (now_us >= ctx->fire_next_us) {
                ctx->fire_target = random_float(0.25f, 1.00f);
                ctx->fire_next_us = now_us + (int64_t)(random_float(40.0f, 120.0f) * 1000.0f);
            }

            ctx->fire_level = step_towards(ctx->fire_level, ctx->fire_target, clamp01(dt_seconds * 10.0f));
            value = ctx->fire_level;
            break;
        }

        case EFFECT_STROBE: {
            float phase_ms = fmodf(time_seconds * 1000.0f, 180.0f);
            value = phase_ms < 45.0f ? 1.0f : 0.0f;
            break;
        }

        case EFFECT_PULSE: {
            float phase_ms = fmodf(time_seconds * 1000.0f, 1300.0f);
            value = pulse_window(phase_ms, 0.0f, 40.0f, 180.0f);
            break;
        }

        case EFFECT_HEARTBEAT: {
            float phase_ms = fmodf(time_seconds * 1000.0f, 1400.0f);
            float first = pulse_window(phase_ms, 0.0f, 35.0f, 140.0f);
            float second = 0.85f * pulse_window(phase_ms, 260.0f, 35.0f, 160.0f);
            value = first + second;
            break;
        }

        case EFFECT_FADE_IN_OUT: {
            float phase = fmodf(time_seconds, 3.0f) / 3.0f;
            value = phase < 0.5f ? (phase * 2.0f) : (2.0f - (phase * 2.0f));
            break;
        }

        case EFFECT_COUNT:
        default:
            value = 1.0f;
            break;
    }

    return clamp01(value);
}

static void effects_task(void *arg)
{
    (void)arg;

    effect_context_t ctx;
    int64_t last_us = esp_timer_get_time();
    light_effect_t last_effect = EFFECT_COUNT;
    bool last_enabled = false;

    reset_context(&ctx, last_us);

    while (true) {
        app_state_snapshot_t snapshot;
        app_state_get_snapshot(&snapshot);

        int64_t now_us = esp_timer_get_time();
        float dt_seconds = (float)(now_us - last_us) / 1000000.0f;
        if (dt_seconds < 0.0f) {
            dt_seconds = 0.0f;
        }
        last_us = now_us;

        if (snapshot.effect != last_effect || snapshot.light_enabled != last_enabled) {
            reset_context(&ctx, now_us);
        }

        float output_percent = 0.0f;
        if (snapshot.light_enabled) {
            float factor = compute_effect(snapshot.effect, &ctx, now_us, dt_seconds);
            output_percent = snapshot.brightness * factor;
        }

        light_pwm_set_percent(output_percent);

        last_effect = snapshot.effect;
        last_enabled = snapshot.light_enabled;

        vTaskDelay(pdMS_TO_TICKS(APP_EFFECT_TASK_PERIOD_MS));
    }
}

esp_err_t effects_start(void)
{
    BaseType_t result = xTaskCreate(
        effects_task,
        "effects_task",
        APP_EFFECT_TASK_STACK_SIZE,
        NULL,
        APP_EFFECT_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create effects task");
        return ESP_FAIL;
    }

    return ESP_OK;
}
