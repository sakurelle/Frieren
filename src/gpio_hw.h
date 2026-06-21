#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t gpio_hw_init(void);
bool gpio_hw_is_key_inserted(void);
bool gpio_hw_is_usb_present(void);
