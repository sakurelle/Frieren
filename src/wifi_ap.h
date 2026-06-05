#pragma once

#include "esp_err.h"

esp_err_t wifi_ap_start(void);
const char *wifi_ap_get_ip_string(void);
