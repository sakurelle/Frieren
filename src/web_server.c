#include "web_server.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_config.h"
#include "app_state.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "web_server";
static httpd_handle_t s_server;

extern const uint8_t _binary_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t _binary_index_html_end[] asm("_binary_index_html_end");

static esp_err_t send_status_json(httpd_req_t *req)
{
    app_state_snapshot_t snapshot;
    app_state_get_snapshot(&snapshot);

    char body[384];
    int written = snprintf(
        body,
        sizeof(body),
        "{\"mode\":\"%s\",\"usb_present\":%s,\"key_inserted\":%s,\"light_enabled\":%s,"
        "\"brightness\":%u,\"effect\":\"%s\",\"pwm_available\":%s,"
        "\"hardware_mode\":\"%s\",\"deep_sleep_enabled\":%s,\"charger_wakeup_enabled\":false}",
        app_state_mode_to_string(snapshot.mode),
        snapshot.usb_present ? "true" : "false",
        snapshot.key_inserted ? "true" : "false",
        snapshot.light_enabled ? "true" : "false",
        snapshot.brightness,
        app_state_effect_to_string(snapshot.effect),
        APP_LED_PWM_AVAILABLE ? "true" : "false",
        APP_HARDWARE_MODE,
        APP_DEEP_SLEEP_ENABLED ? "true" : "false"
    );

    if (written < 0 || written >= (int)sizeof(body)) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_bad_request(httpd_req_t *req, const char *message)
{
    char body[128];
    int written = snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    if (written < 0 || written >= (int)sizeof(body)) {
        return ESP_FAIL;
    }

    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    size_t html_size = (size_t)(_binary_index_html_end - _binary_index_html_start);
    if (html_size > 0) {
        html_size -= 1;
    }

    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)_binary_index_html_start, html_size);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    return send_status_json(req);
}

static esp_err_t set_get_handler(httpd_req_t *req)
{
    char query[128];
    esp_err_t err = httpd_req_get_url_query_str(req, query, sizeof(query));
    if (err != ESP_OK) {
        return send_status_json(req);
    }

    bool has_brightness = false;
    bool has_effect = false;
    int brightness = 0;
    light_effect_t effect = EFFECT_STATIC;

    char brightness_value[8];
    if (httpd_query_key_value(query, "brightness", brightness_value, sizeof(brightness_value)) == ESP_OK) {
        char *end_ptr = NULL;
        long parsed = strtol(brightness_value, &end_ptr, 10);
        if (end_ptr == brightness_value || *end_ptr != '\0' || parsed < 0 || parsed > 100) {
            return send_bad_request(req, "invalid brightness");
        }
        brightness = (int)parsed;
        has_brightness = true;
    }

    char effect_value[32];
    if (httpd_query_key_value(query, "effect", effect_value, sizeof(effect_value)) == ESP_OK) {
        if (!app_state_effect_from_string(effect_value, &effect)) {
            return send_bad_request(req, "invalid effect");
        }
        has_effect = true;
    }

    err = app_state_set_settings(brightness, effect, has_brightness, has_effect);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to persist settings: %s", esp_err_to_name(err));
    }

    return send_status_json(req);
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = APP_HTTP_PORT;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        return err;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t set_uri = {
        .uri = "/api/set",
        .method = HTTP_GET,
        .handler = set_get_handler,
        .user_ctx = NULL
    };

    err = httpd_register_uri_handler(s_server, &index_uri);
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(s_server, &status_uri);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(s_server, &set_uri);
    }

    if (err != ESP_OK) {
        httpd_stop(s_server);
        s_server = NULL;
        return err;
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", APP_HTTP_PORT);
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    esp_err_t err = httpd_stop(s_server);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server stopped");
        s_server = NULL;
    }

    return err;
}
