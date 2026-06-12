#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t gpio_hw_init(void);
bool gpio_hw_is_key_inserted(void);
bool gpio_hw_is_usb_present(void);
esp_err_t gpio_hw_set_boost_enabled(bool enabled);
bool gpio_hw_is_boost_enabled(void);
