#pragma once

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#define BOARD_WIFI_SSID_CFG WIFI_SSID
#define BOARD_WIFI_PASS_CFG WIFI_PASSWORD
#else
#define BOARD_WIFI_SSID_CFG CONFIG_BOARD_WIFI_SSID
#define BOARD_WIFI_PASS_CFG CONFIG_BOARD_WIFI_PASSWORD
#endif
