#include "wifi_ap.h"

#include <string.h>
#include "app_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "wifi_ap";
static esp_netif_t *s_ap_netif;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != WIFI_EVENT) {
        return;
    }

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Client connected to SoftAP");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "Client disconnected from SoftAP");
    }
}

esp_err_t wifi_ap_start(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_ap_netif == NULL) {
        return ESP_FAIL;
    }

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 4);
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 4);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    err = esp_netif_dhcps_stop(s_ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return err;
    }

    err = esp_netif_set_ip_info(s_ap_netif, &ip_info);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_dhcps_start(s_ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        return err;
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifi_config = {
        .ap = {
            .channel = 1,
            .max_connection = APP_WIFI_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0
        }
    };

    memcpy(wifi_config.ap.ssid, APP_SOFTAP_SSID, strlen(APP_SOFTAP_SSID));
    wifi_config.ap.ssid_len = strlen(APP_SOFTAP_SSID);

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "SoftAP started: SSID=%s IP=%s", APP_SOFTAP_SSID, APP_SOFTAP_IP);
    return ESP_OK;
}

const char *wifi_ap_get_ip_string(void)
{
    return APP_SOFTAP_IP;
}
