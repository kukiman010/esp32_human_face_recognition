#include "web_server.h"

#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "face_preview.h"
#include "freertos/FreeRTOS.h"
#include "mdns.h"

#include <cstdio>

static const char *TAG = "web_server";
static httpd_handle_t s_server = nullptr;
static bool s_started = false;

static const char INDEX_HTML[] =
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Face Cam</title>"
    "<style>body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:16px}"
    "img{max-width:100%;border:2px solid #444}#st{margin-top:8px;color:#9cf}</style></head><body>"
    "<h2>Face recognition</h2>"
    "<p>Yellow box = unknown, green = recognized</p>"
    "<img id='v' alt='camera'>"
    "<div id='st'>loading...</div>"
    "<script>"
    "function tick(){"
    " fetch('/status').then(r=>r.json()).then(j=>{"
    "  document.getElementById('st').textContent="
    "   'faces='+j.faces+' id='+(j.id||'-')+' sim='+(j.sim||'-');"
    " }).catch(()=>{});"
    " document.getElementById('v').src='/jpg?t='+Date.now();"
    "}"
    "setInterval(tick,500);tick();"
    "</script></body></html>";

static esp_err_t register_uri(httpd_handle_t server, httpd_uri_t *uri)
{
    esp_err_t err = httpd_register_uri_handler(server, uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register %s failed: %s", uri->uri, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ping_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /ping");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "pong", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_not_ready(httpd_req_t *req)
{
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "frame not ready", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t jpg_handler(httpd_req_t *req)
{
    size_t need = face_preview_jpeg_size();
    if (need == 0) {
        ESP_LOGD(TAG, "GET /jpg (waiting for first frame)");
        return send_not_ready(req);
    }

    need += 4096;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t len = 0;
    if (!face_preview_copy_jpeg(buf, need, &len)) {
        heap_caps_free(buf);
        return send_not_ready(req);
    }

    ESP_LOGD(TAG, "GET /jpg (%u bytes)", (unsigned)len);
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, (const char *)buf, len);
    heap_caps_free(buf);
    return err;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    size_t faces = 0;
    int id = -1;
    float sim = 0.0f;
    face_preview_get_status(&faces, &id, &sim);

    char json[128];
    if (id > 0) {
        snprintf(json, sizeof(json), "{\"faces\":%u,\"id\":%d,\"sim\":%.3f}", (unsigned)faces, id, sim);
    } else {
        snprintf(json, sizeof(json), "{\"faces\":%u,\"id\":null,\"sim\":null}", (unsigned)faces);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static void start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_init: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set("esp32cam");
    mdns_instance_name_set("Face Recognition Cam");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS: http://esp32cam.local/");
}

static void log_urls(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta) {
        esp_netif_ip_info_t ip = {};
        esp_netif_get_ip_info(sta, &ip);
        ESP_LOGI(TAG, "STA: http://" IPSTR "/ping", IP2STR(&ip.ip));
    }

#if CONFIG_BOARD_WIFI_AP_ENABLE
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap) {
        esp_netif_ip_info_t ip = {};
        esp_netif_get_ip_info(ap, &ip);
        ESP_LOGI(TAG, "AP:  http://" IPSTR "/ping  (WiFi: %s)", IP2STR(&ip.ip), CONFIG_BOARD_WIFI_AP_SSID);
    }
#endif
}

esp_err_t web_server_start(void)
{
#if !CONFIG_BOARD_WEB_ENABLE
    return ESP_OK;
#endif

    if (s_started) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 12288;
    config.core_id = 0;
    config.task_priority = tskIDLE_PRIORITY + 8;
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = nullptr};
    httpd_uri_t ping = {.uri = "/ping", .method = HTTP_GET, .handler = ping_handler, .user_ctx = nullptr};
    httpd_uri_t jpg = {.uri = "/jpg", .method = HTTP_GET, .handler = jpg_handler, .user_ctx = nullptr};
    httpd_uri_t status = {.uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = nullptr};
    register_uri(s_server, &root);
    register_uri(s_server, &ping);
    register_uri(s_server, &jpg);
    register_uri(s_server, &status);

    start_mdns();
    log_urls();
    ESP_LOGI(TAG, "HTTP server listening on port 80 (core 0)");

    s_started = true;
    return ESP_OK;
}
