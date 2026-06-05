#pragma once

#include "esp_err.h"

esp_err_t light_pwm_init(void);
esp_err_t light_pwm_set_percent(float percent);
