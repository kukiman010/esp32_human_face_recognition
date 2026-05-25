#include "board_wifi.h"
#include "board_wifi_config.h"
#include "web_server.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "board_wifi";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_events;
static bool s_web_launch_task_created = false;

static void web_server_launch_task(void *arg)
{
    (void)arg;
    xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_err_t err = web_server_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "web_server_start failed: %s", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}

static void schedule_web_server_start(void)
{
    if (s_web_launch_task_created) {
        return;
    }
    s_web_launch_task_created = true;
    xTaskCreate(web_server_launch_task, "web_start", 4096, NULL, 5, NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected, retrying...");
        xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        schedule_web_server_start();
    }
}

esp_err_t board_wifi_init(void)
{
#if !CONFIG_BOARD_WEB_ENABLE
    return ESP_OK;
#endif

    const char *ssid = BOARD_WIFI_SSID_CFG;
    const char *pass = BOARD_WIFI_PASS_CFG;

    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "WiFi SSID is empty. Create main/wifi_secrets.h from wifi_secrets.h.example "
                      "(or set menuconfig: Example -> human_face_recognition -> WiFi SSID)");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to '%s'...", ssid);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
#if CONFIG_BOARD_WIFI_AP_ENABLE
    esp_netif_create_default_wifi_ap();
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass ? pass : "", sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

#if CONFIG_BOARD_WIFI_AP_ENABLE
    wifi_config_t ap_config = {};
    const char *ap_ssid = CONFIG_BOARD_WIFI_AP_SSID;
    const char *ap_pass = CONFIG_BOARD_WIFI_AP_PASSWORD;
    strncpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(ap_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if (ap_pass && strlen(ap_pass) >= 8) {
        strncpy((char *)ap_config.ap.password, ap_pass, sizeof(ap_config.ap.password) - 1);
    } else {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_LOGI(TAG, "SoftAP '%s' -> http://192.168.4.1/ (password: %s)",
             ap_ssid, ap_config.ap.authmode == WIFI_AUTH_OPEN ? "(open)" : ap_pass);
#else
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
#endif

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(20000));
    if (bits & WIFI_CONNECTED_BIT) {
        schedule_web_server_start();
        return ESP_OK;
    }

    ESP_LOGW(TAG, "WiFi still connecting — HTTP starts when IP is assigned");
    return ESP_OK;
}
