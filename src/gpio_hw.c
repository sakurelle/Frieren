#include "gpio_hw.h"

#include "app_config.h"
#include "driver/gpio.h"

static bool s_boost_enabled;

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

    gpio_config_t boost_cfg = {
        .pin_bit_mask = 1ULL << APP_GPIO_BOOST_EN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&key_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_config(&usb_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_config(&boost_cfg);
    if (err != ESP_OK) {
        return err;
    }

    s_boost_enabled = false;
    return gpio_set_level(APP_GPIO_BOOST_EN, 0);
}

bool gpio_hw_is_key_inserted(void)
{
    return gpio_get_level(APP_GPIO_KEY_SENSE) == 0;
}

bool gpio_hw_is_usb_present(void)
{
    return gpio_get_level(APP_GPIO_USB_PRESENT) == 1;
}

esp_err_t gpio_hw_set_boost_enabled(bool enabled)
{
    esp_err_t err = gpio_set_level(APP_GPIO_BOOST_EN, enabled ? 1 : 0);
    if (err == ESP_OK) {
        s_boost_enabled = enabled;
    }

    return err;
}

bool gpio_hw_is_boost_enabled(void)
{
    return s_boost_enabled;
}
