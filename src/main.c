#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define GPIO_KEY_SENSE      GPIO_NUM_5
#define GPIO_USB_PRESENT    GPIO_NUM_7
#define GPIO_MOSFET_GATE    GPIO_NUM_6

static const char *TAG = "TYPEC_CTRL";

static int read_key_inserted(void)
{
    return gpio_get_level(GPIO_KEY_SENSE) == 0;
}

static int read_usb_present(void)
{
    return gpio_get_level(GPIO_USB_PRESENT) == 1;
}

static void set_light_power(int enabled)
{
    gpio_set_level(GPIO_MOSFET_GATE, enabled ? 1 : 0);
}

static void print_status(int key_inserted, int usb_present, int light_enabled)
{
    ESP_LOGI(TAG,
             "STATUS: USB_5V=%s | KEY=%s | LIGHT=%s | MODE=%s",
             usb_present ? "PRESENT" : "ABSENT",
             key_inserted ? "INSERTED" : "REMOVED",
             light_enabled ? "ON" : "OFF",
             usb_present ? "CHARGING_KEY_IGNORED" :
             key_inserted ? "KEY_ACTIVE" : "IDLE");
}

static void configure_gpio(void)
{
    gpio_config_t key_cfg = {
        .pin_bit_mask = 1ULL << GPIO_KEY_SENSE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config_t usb_cfg = {
        .pin_bit_mask = 1ULL << GPIO_USB_PRESENT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config_t mosfet_cfg = {
        .pin_bit_mask = 1ULL << GPIO_MOSFET_GATE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&key_cfg);
    gpio_config(&usb_cfg);
    gpio_config(&mosfet_cfg);

    set_light_power(0);
}

void app_main(void)
{
    configure_gpio();

    ESP_LOGI(TAG, "ESP32-C3 Type-C key + charging detection + IRLZ44N LED control");
    ESP_LOGI(TAG, "GPIO5 <- Type-C D+");
    ESP_LOGI(TAG, "GPIO7 <- Type-C 5V divider");
    ESP_LOGI(TAG, "GPIO6 -> IRLZ44N Gate");

    int stable_key = read_key_inserted();
    int stable_usb = read_usb_present();

    int light_enabled = (!stable_usb && stable_key);
    set_light_power(light_enabled);
    print_status(stable_key, stable_usb, light_enabled);

    int seconds = 0;

    while (1) {
        int current_key = read_key_inserted();
        int current_usb = read_usb_present();

        if (current_key != stable_key || current_usb != stable_usb) {
            vTaskDelay(pdMS_TO_TICKS(150));

            current_key = read_key_inserted();
            current_usb = read_usb_present();

            if (current_key != stable_key || current_usb != stable_usb) {
                stable_key = current_key;
                stable_usb = current_usb;

                light_enabled = (!stable_usb && stable_key);
                set_light_power(light_enabled);

                if (stable_usb) {
                    ESP_LOGI(TAG, "USB POWER PRESENT: charging input connected");
                    ESP_LOGI(TAG, "Light disabled while charging input is present");
                } else {
                    ESP_LOGI(TAG, "USB POWER ABSENT");

                    if (stable_key) {
                        ESP_LOGI(TAG, "KEY INSERTED: IRLZ44N ON, LED should turn ON");
                    } else {
                        ESP_LOGI(TAG, "KEY REMOVED: IRLZ44N OFF, LED should turn OFF");
                    }
                }

                print_status(stable_key, stable_usb, light_enabled);
            }
        }

        seconds++;

        if (seconds >= 5) {
            seconds = 0;

            stable_key = read_key_inserted();
            stable_usb = read_usb_present();

            light_enabled = (!stable_usb && stable_key);
            set_light_power(light_enabled);

            print_status(stable_key, stable_usb, light_enabled);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
