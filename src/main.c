#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "app_state.h"
#include "effects.h"
#include "gpio_hw.h"
#include "light_pwm.h"
#include "web_server.h"
#include "wifi_ap.h"

static const char *TAG = "frieren_main";

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    return err;
}

static void log_snapshot(const char *prefix)
{
    app_state_snapshot_t snapshot;
    app_state_get_snapshot(&snapshot);

    ESP_LOGI(TAG,
             "%s mode=%s usb=%s key=%s boost=%s light=%s brightness=%u effect=%s",
             prefix,
             app_state_mode_to_string(snapshot.mode),
             snapshot.usb_present ? "PRESENT" : "ABSENT",
             snapshot.key_inserted ? "INSERTED" : "REMOVED",
             snapshot.boost_enabled ? "ENABLED" : "DISABLED",
             snapshot.light_enabled ? "ON" : "OFF",
             snapshot.brightness,
             app_state_effect_to_string(snapshot.effect));
}

static void inputs_task(void *arg)
{
    (void)arg;

    app_mode_t prev_mode = MODE_IDLE;
    app_mode_t current_mode = MODE_IDLE;

    while (true) {
        bool usb_present = gpio_hw_is_usb_present();
        bool key_inserted = gpio_hw_is_key_inserted();

        bool mode_changed = app_state_update_inputs(
            usb_present,
            key_inserted,
            &prev_mode,
            &current_mode
        );

        if (mode_changed) {
            ESP_LOGI(TAG,
                     "MODE changed: %s -> %s",
                     app_state_mode_to_string(prev_mode),
                     app_state_mode_to_string(current_mode));

            log_snapshot("STATE:");
        }

        vTaskDelay(pdMS_TO_TICKS(APP_INPUT_POLL_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting %s", APP_PROJECT_NAME);
    ESP_LOGI(TAG, "Hardware mode: %s", APP_HARDWARE_MODE);
    ESP_LOGI(TAG, "Configured Web UI IP: http://%s/", APP_SOFTAP_IP);

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(app_state_init());
    ESP_ERROR_CHECK(gpio_hw_init());
    ESP_ERROR_CHECK(light_pwm_init());

    bool usb_present = gpio_hw_is_usb_present();
    bool key_inserted = gpio_hw_is_key_inserted();

    app_state_update_inputs(usb_present, key_inserted, NULL, NULL);
    log_snapshot("Initial state:");

    ESP_ERROR_CHECK(wifi_ap_start());
    ESP_ERROR_CHECK(web_server_start());
    ESP_ERROR_CHECK(effects_start());

    ESP_LOGI(TAG, "Wi-Fi AP SSID: %s", APP_SOFTAP_SSID);
    ESP_LOGI(TAG, "Wi-Fi password: %s", APP_SOFTAP_PASSWORD);
    ESP_LOGI(TAG, "Web UI: http://%s/", wifi_ap_get_ip_string());
    ESP_LOGI(TAG,
             "GPIO key=%d usb=%d led_pwm=%d boost_en=%d",
             APP_GPIO_KEY_SENSE,
             APP_GPIO_USB_PRESENT,
             APP_GPIO_LED_PWM,
             APP_GPIO_BOOST_EN);

    BaseType_t task_result = xTaskCreate(
        inputs_task,
        "inputs_task",
        3072,
        NULL,
        6,
        NULL
    );

    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create inputs_task");
        abort();
    }

    while (true) {
        log_snapshot("Periodic:");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
