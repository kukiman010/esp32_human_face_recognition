#include "board_camera.h"

#include "board_config.h"
#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "board_camera";

esp_err_t board_camera_init(void)
{
    camera_config_t config = BOARD_CAMERA_CONFIG;
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return err;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_vflip(sensor, 0);
        sensor->set_hmirror(sensor, 0);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
        ESP_LOGI(TAG, "probe OK: %ux%u fmt=%d", fb->width, fb->height, fb->format);
        esp_camera_fb_return(fb);
    } else {
        ESP_LOGW(TAG, "camera init OK but probe frame failed");
    }

    return ESP_OK;
}

esp_err_t board_camera_deinit(void)
{
    return esp_camera_deinit();
}
