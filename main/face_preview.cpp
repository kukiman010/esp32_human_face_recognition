#include "face_preview.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "img_converters.h"

#include <cstring>
#include <vector>

static const char *TAG = "face_preview";

static SemaphoreHandle_t s_lock;
static uint8_t *s_jpeg = nullptr;
static size_t s_jpeg_len = 0;
static int64_t s_last_update_us = 0;
static size_t s_last_faces = 0;
static int s_last_matched_id = -1;
static float s_last_matched_sim = 0.0f;

static void draw_rect_rgb565(uint16_t *pixels, int width, int height, int x0, int y0, int x1, int y1, uint16_t color)
{
    const int thick = 2;
    for (int t = 0; t < thick; t++) {
        int y_top = y0 + t;
        int y_bot = y1 - t;
        for (int x = x0; x <= x1; x++) {
            if (x >= 0 && x < width) {
                if (y_top >= 0 && y_top < height) {
                    pixels[y_top * width + x] = color;
                }
                if (y_bot >= 0 && y_bot < height) {
                    pixels[y_bot * width + x] = color;
                }
            }
        }
        int x_left = x0 + t;
        int x_right = x1 - t;
        for (int y = y0; y <= y1; y++) {
            if (y >= 0 && y < height) {
                if (x_left >= 0 && x_left < width) {
                    pixels[y * width + x_left] = color;
                }
                if (x_right >= 0 && x_right < width) {
                    pixels[y * width + x_right] = color;
                }
            }
        }
    }
}

void face_preview_init(void)
{
    s_lock = xSemaphoreCreateMutex();
}

void face_preview_update(camera_fb_t *fb,
                         const std::list<dl::detect::result_t> &faces,
                         int matched_id,
                         float matched_sim)
{
#if !CONFIG_BOARD_WEB_ENABLE
    (void)fb;
    (void)faces;
    (void)matched_id;
    (void)matched_sim;
    return;
#endif

    if (!fb || fb->format != PIXFORMAT_RGB565) {
        return;
    }

    int64_t now = esp_timer_get_time();
    if (now - s_last_update_us < 300000) {
        return;
    }
    s_last_update_us = now;

    const size_t rgb565_bytes = (size_t)fb->width * fb->height * 2;
    uint16_t *scratch = (uint16_t *)heap_caps_malloc(rgb565_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!scratch) {
        return;
    }
    memcpy(scratch, fb->buf, rgb565_bytes);

    for (const auto &face : faces) {
        if (face.box.size() < 4) {
            continue;
        }
        bool recognized = matched_id > 0;
        uint16_t color = recognized ? 0x07E0 : 0xFFE0;
        draw_rect_rgb565(scratch, fb->width, fb->height, face.box[0], face.box[1], face.box[2], face.box[3], color);
    }

    uint8_t *jpeg = nullptr;
    size_t jpeg_len = 0;
    if (!fmt2jpg((uint8_t *)scratch, rgb565_bytes, fb->width, fb->height, PIXFORMAT_RGB565, 12, &jpeg, &jpeg_len)) {
        heap_caps_free(scratch);
        ESP_LOGW(TAG, "fmt2jpg failed");
        return;
    }
    heap_caps_free(scratch);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_jpeg) {
        heap_caps_free(s_jpeg);
    }
    s_jpeg = jpeg;
    s_jpeg_len = jpeg_len;
    s_last_faces = faces.size();
    s_last_matched_id = matched_id;
    s_last_matched_sim = matched_sim;
    xSemaphoreGive(s_lock);
}

void face_preview_get_status(size_t *faces, int *matched_id, float *matched_sim)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (faces) {
        *faces = s_last_faces;
    }
    if (matched_id) {
        *matched_id = s_last_matched_id;
    }
    if (matched_sim) {
        *matched_sim = s_last_matched_sim;
    }
    xSemaphoreGive(s_lock);
}

bool face_preview_copy_jpeg(uint8_t *out_buf, size_t out_buf_size, size_t *out_len)
{
    if (!out_buf || !out_len) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_jpeg || s_jpeg_len == 0 || s_jpeg_len > out_buf_size) {
        xSemaphoreGive(s_lock);
        return false;
    }
    memcpy(out_buf, s_jpeg, s_jpeg_len);
    *out_len = s_jpeg_len;
    xSemaphoreGive(s_lock);
    return true;
}

size_t face_preview_jpeg_size(void)
{
    size_t len = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    len = s_jpeg_len;
    xSemaphoreGive(s_lock);
    return len;
}
