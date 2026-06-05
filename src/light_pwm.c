#include "light_pwm.h"

#include <math.h>
#include "app_config.h"
#include "driver/ledc.h"

static float clamp_percent(float percent)
{
    if (percent < 0.0f) {
        return 0.0f;
    }

    if (percent > 100.0f) {
        return 100.0f;
    }

    return percent;
}

esp_err_t light_pwm_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = APP_PWM_MODE,
        .timer_num = APP_PWM_TIMER,
        .duty_resolution = APP_PWM_RESOLUTION,
        .freq_hz = APP_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        return err;
    }

    ledc_channel_config_t channel_config = {
        .gpio_num = APP_GPIO_LIGHT_PWM,
        .speed_mode = APP_PWM_MODE,
        .channel = APP_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = APP_PWM_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    return ledc_channel_config(&channel_config);
}

esp_err_t light_pwm_set_percent(float percent)
{
    float normalized = clamp_percent(percent);
    uint32_t duty = (uint32_t)lroundf((normalized / 100.0f) * APP_PWM_DUTY_MAX);

    esp_err_t err = ledc_set_duty(APP_PWM_MODE, APP_PWM_CHANNEL, duty);
    if (err != ESP_OK) {
        return err;
    }

    return ledc_update_duty(APP_PWM_MODE, APP_PWM_CHANNEL);
}
