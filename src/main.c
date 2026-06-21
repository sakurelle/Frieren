#include <inttypes.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
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

static const char *wakeup_cause_to_string(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return "UNDEFINED";
        case ESP_SLEEP_WAKEUP_EXT0:
            return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "EXT1";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "TIMER";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO:
            return "GPIO";
        case ESP_SLEEP_WAKEUP_UART:
            return "UART";
        case ESP_SLEEP_WAKEUP_UART1:
            return "UART1";
        case ESP_SLEEP_WAKEUP_UART2:
            return "UART2";
        case ESP_SLEEP_WAKEUP_WIFI:
            return "WIFI";
        case ESP_SLEEP_WAKEUP_COCPU:
            return "COCPU";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            return "COCPU_TRAP";
        case ESP_SLEEP_WAKEUP_BT:
            return "BT";
        default:
            return "OTHER";
    }
}

static void log_wakeup_info(void)
{
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    uint64_t gpio_mask = esp_sleep_get_gpio_wakeup_status();

    ESP_LOGI(TAG, "Wakeup cause: %s (%d)", wakeup_cause_to_string(cause), (int)cause);
    ESP_LOGI(TAG, "GPIO wakeup status mask: 0x%016" PRIx64, gpio_mask);
}

static void log_snapshot(const char *prefix)
{
    app_state_snapshot_t snapshot;
    app_state_get_snapshot(&snapshot);

    ESP_LOGI(TAG,
             "%s mode=%s usb=%s key=%s light=%s brightness=%u effect=%s",
             prefix,
             app_state_mode_to_string(snapshot.mode),
             snapshot.usb_present ? "PRESENT" : "ABSENT",
             snapshot.key_inserted ? "INSERTED" : "REMOVED",
             snapshot.light_enabled ? "ON" : "OFF",
             snapshot.brightness,
             app_state_effect_to_string(snapshot.effect));
}

static bool deep_sleep_wakeup_gpio_valid(gpio_num_t gpio_num, const char *label)
{
    bool valid = esp_sleep_is_valid_wakeup_gpio(gpio_num);
    if (!valid) {
        ESP_LOGE(TAG,
                 "%s GPIO%d is not valid for deep sleep wakeup on this target. Staying awake to avoid losing the board.",
                 label,
                 (int)gpio_num);
    }

    return valid;
}

static bool prepare_gpio_wakeup(void)
{
#if APP_DEEP_SLEEP_ENABLED
    if (!deep_sleep_wakeup_gpio_valid(APP_GPIO_KEY_SENSE, "KEY_SENSE")) {
        return false;
    }

    if (!deep_sleep_wakeup_gpio_valid(APP_GPIO_USB_PRESENT, "USB_PRESENT")) {
        return false;
    }

    esp_err_t err = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear wakeup sources: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(1ULL << APP_GPIO_KEY_SENSE, ESP_GPIO_WAKEUP_GPIO_LOW);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable GPIO%d LOW wakeup: %s", (int)APP_GPIO_KEY_SENSE, esp_err_to_name(err));
        return false;
    }

    err = esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(1ULL << APP_GPIO_USB_PRESENT, ESP_GPIO_WAKEUP_GPIO_HIGH);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable GPIO%d HIGH wakeup: %s", (int)APP_GPIO_USB_PRESENT, esp_err_to_name(err));
        return false;
    }

    return true;
#else
    return false;
#endif
}

static bool enter_deep_sleep_if_possible(void)
{
#if APP_DEEP_SLEEP_ENABLED
    if (!prepare_gpio_wakeup()) {
        return false;
    }

    esp_err_t err = light_pwm_set_percent(0.0f);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set PWM to 0 before sleep: %s", esp_err_to_name(err));
    }

    err = web_server_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop web server: %s", esp_err_to_name(err));
    }

    err = wifi_ap_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop Wi-Fi AP: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG,
             "Entering deep sleep. Wakeup sources: GPIO%d LOW (key), GPIO%d HIGH (USB 5V)",
             (int)APP_GPIO_KEY_SENSE,
             (int)APP_GPIO_USB_PRESENT);

    vTaskDelay(pdMS_TO_TICKS(150));
    esp_deep_sleep_start();
    return true;
#else
    ESP_LOGI(TAG, "Deep sleep is disabled at compile time");
    return false;
#endif
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
    int64_t idle_since_us = 0;
    int64_t last_log_us = 0;
    bool sleep_blocked_in_idle = false;

    ESP_LOGI(TAG, "Starting %s", APP_PROJECT_NAME);
    ESP_LOGI(TAG, "Hardware mode: %s", APP_HARDWARE_MODE);
    ESP_LOGI(TAG, "Configured Web UI IP: http://%s/", APP_SOFTAP_IP);
    ESP_LOGI(TAG, "Deep sleep enabled: %s", APP_DEEP_SLEEP_ENABLED ? "true" : "false");
    log_wakeup_info();

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
             "GPIO key=%d usb=%d led_pwm=%d",
             APP_GPIO_KEY_SENSE,
             APP_GPIO_USB_PRESENT,
             APP_GPIO_LED_PWM);

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
        app_state_snapshot_t snapshot;
        app_state_get_snapshot(&snapshot);

        int64_t now_us = esp_timer_get_time();

        if (snapshot.mode == MODE_IDLE) {
            if (idle_since_us == 0) {
                idle_since_us = now_us;
                sleep_blocked_in_idle = false;
                ESP_LOGI(TAG, "MODE_IDLE detected, deep sleep timer started (%d ms)", APP_SLEEP_DELAY_MS);
            }

#if APP_DEEP_SLEEP_ENABLED
            if (!sleep_blocked_in_idle &&
                (now_us - idle_since_us) >= ((int64_t)APP_SLEEP_DELAY_MS * 1000)) {
                if (!enter_deep_sleep_if_possible()) {
                    sleep_blocked_in_idle = true;
                    ESP_LOGW(TAG, "Deep sleep skipped, device will remain awake in MODE_IDLE");
                }
            }
#endif
        } else {
            idle_since_us = 0;
            sleep_blocked_in_idle = false;
        }

        if (last_log_us == 0 || (now_us - last_log_us) >= 5000000) {
            log_snapshot("Periodic:");
            last_log_us = now_us;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
