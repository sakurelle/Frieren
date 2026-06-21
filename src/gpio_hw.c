#include "gpio_hw.h"

#include "app_config.h"
#include "driver/gpio.h"

esp_err_t gpio_hw_init(void)
{
    gpio_config_t key_cfg = {
        .pin_bit_mask = 1ULL << APP_GPIO_KEY_SENSE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config_t usb_cfg = {
        .pin_bit_mask = 1ULL << APP_GPIO_USB_PRESENT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&key_cfg);
    if (err != ESP_OK) {
        return err;
    }

    return gpio_config(&usb_cfg);
}

bool gpio_hw_is_key_inserted(void)
{
    return gpio_get_level(APP_GPIO_KEY_SENSE) == 0;
}

bool gpio_hw_is_usb_present(void)
{
    return gpio_get_level(APP_GPIO_USB_PRESENT) == 1;
}
