#include "board_camera.h"

#include "board_config.h"
#include "esp_heap_caps.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "img_converters.h"

#include <cinttypes>

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
        sensor->set_brightness(sensor, 1);
        sensor->set_contrast(sensor, 0);
        sensor->set_saturation(sensor, 0);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
        ESP_LOGI(TAG, "probe OK: %ux%u fmt=%d len=%zu", fb->width, fb->height, fb->format, fb->len);
        uint8_t *rgb = (uint8_t *)heap_caps_malloc(fb->width * fb->height * 3, MALLOC_CAP_SPIRAM);
        if (rgb && fmt2rgb888(fb->buf, fb->len, fb->format, rgb)) {
            uint64_t sum = 0;
            size_t pixels = fb->width * fb->height;
            for (size_t i = 0; i < pixels; i++) {
                sum += rgb[i * 3] + rgb[i * 3 + 1] + rgb[i * 3 + 2];
            }
            ESP_LOGI(TAG, "probe brightness avg=%" PRIu32 " (0=black, ~128=ok)", (uint32_t)(sum / (pixels * 3)));
            heap_caps_free(rgb);
        } else {
            ESP_LOGW(TAG, "probe fmt2rgb888 failed");
        }
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
