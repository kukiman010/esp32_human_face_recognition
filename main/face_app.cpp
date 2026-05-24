#include "face_app.h"

#include "board_camera.h"
#include "board_config.h"
#include "board_status_led.h"
#include "dl_image_jpeg.hpp"
#include "esp_camera.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <string>
#include <strings.h>
#include <vector>

static const char *TAG = "face_app";

struct FaceAppContext {
    HumanFaceDetect *detector;
    HumanFaceRecognizer *recognizer;
    SemaphoreHandle_t lock;
    volatile bool enroll_requested;
    std::string db_path;
};

static FaceAppContext s_app;

static dl::image::img_t frame_to_img(camera_fb_t *fb)
{
    return {
        .data = fb->buf,
        .width = (uint16_t)fb->width,
        .height = (uint16_t)fb->height,
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE,
    };
}

static void trim_line(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

static void print_help(void)
{
    printf(
        "\nCommands:\n"
        "  enroll   - register face from current camera frame\n"
        "  list     - show enrolled faces count\n"
        "  clear    - delete all enrolled faces\n"
        "  delete N - delete face id N\n"
        "  help     - this message\n"
        "\nEnroll from photo: copy .jpg to /sdcard/enroll/ and reboot.\n"
        "BOOT button (GPIO0): enroll next detected face.\n\n");
}

static void enroll_from_img(FaceAppContext *app, const dl::image::img_t &img, const char *source)
{
    xSemaphoreTake(app->lock, portMAX_DELAY);
    auto &faces = app->detector->run(img);
    esp_err_t err = app->recognizer->enroll(img, faces);
    int count = app->recognizer->get_num_feats();
    xSemaphoreGive(app->lock);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Enrolled from %s, total faces: %d", source, count);
    } else {
        ESP_LOGW(TAG, "Enroll failed (%s): no face in image", source);
    }
}

static bool read_file_bytes(const std::filesystem::path &path, std::vector<uint8_t> &out)
{
    FILE *f = fopen(path.string().c_str(), "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_SET);
    out.resize((size_t)size);
    if (fread(out.data(), 1, out.size(), f) != out.size()) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static void enroll_jpegs_from_dir(FaceAppContext *app, const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGI(TAG, "Optional enroll folder missing: %s", dir_path);
        return;
    }

    ESP_LOGI(TAG, "Scanning %s for .jpg enroll photos...", dir_path);
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        const char *dot = strrchr(name, '.');
        if (!dot) {
            continue;
        }
        if (strcasecmp(dot, ".jpg") != 0 && strcasecmp(dot, ".jpeg") != 0) {
            continue;
        }

        auto path = std::filesystem::path(dir_path) / name;
        std::vector<uint8_t> raw;
        if (!read_file_bytes(path, raw)) {
            ESP_LOGW(TAG, "Cannot read %s", path.c_str());
            continue;
        }

        dl::image::jpeg_img_t jpeg = {.data = raw.data(), .data_len = raw.size()};
        auto img = dl::image::sw_decode_jpeg(jpeg, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
        if (!img.data) {
            ESP_LOGW(TAG, "JPEG decode failed: %s", name);
            continue;
        }

        enroll_from_img(app, img, name);
        heap_caps_free(img.data);
    }
    closedir(dir);
}

static void process_command(FaceAppContext *app, char *line)
{
    trim_line(line);
    if (line[0] == '\0') {
        return;
    }

    if (strcmp(line, "help") == 0) {
        print_help();
        return;
    }

    if (strcmp(line, "enroll") == 0) {
        app->enroll_requested = true;
        ESP_LOGI(TAG, "Enroll requested — look at the camera");
        return;
    }

    if (strcmp(line, "list") == 0) {
        xSemaphoreTake(app->lock, portMAX_DELAY);
        int n = app->recognizer->get_num_feats();
        xSemaphoreGive(app->lock);
        printf("Enrolled faces: %d (database: %s)\n", n, app->db_path.c_str());
        return;
    }

    if (strcmp(line, "clear") == 0) {
        xSemaphoreTake(app->lock, portMAX_DELAY);
        app->recognizer->clear_all_feats();
        xSemaphoreGive(app->lock);
        ESP_LOGI(TAG, "All faces cleared");
        return;
    }

    if (strncmp(line, "delete ", 7) == 0) {
        int id = atoi(line + 7);
        if (id <= 0) {
            printf("Usage: delete <id>\n");
            return;
        }
        xSemaphoreTake(app->lock, portMAX_DELAY);
        esp_err_t err = app->recognizer->delete_feat((uint16_t)id);
        xSemaphoreGive(app->lock);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Deleted face id %d", id);
        } else {
            ESP_LOGW(TAG, "Delete id %d failed", id);
        }
        return;
    }

    printf("Unknown command: %s (type 'help')\n", line);
}

static void stdin_task(void *arg)
{
    auto *app = static_cast<FaceAppContext *>(arg);
    setvbuf(stdin, NULL, _IONBF, 0);
    print_help();

    char line[128];
    while (true) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        process_command(app, line);
    }
}

static void poll_boot_button(FaceAppContext *app)
{
    static bool prev_pressed = false;
    static int64_t last_trigger_us = 0;

    bool pressed = gpio_get_level(BOARD_BOOT_BUTTON_GPIO) == 0;
    int64_t now = esp_timer_get_time();

    if (pressed && !prev_pressed && (now - last_trigger_us) > 800000) {
        app->enroll_requested = true;
        last_trigger_us = now;
        ESP_LOGI(TAG, "BOOT button — enroll next face");
    }
    prev_pressed = pressed;
}

esp_err_t face_app_run(const char *db_path)
{
    s_app.db_path = db_path;
    s_app.lock = xSemaphoreCreateMutex();
    s_app.detector = new HumanFaceDetect();
    s_app.recognizer = new HumanFaceRecognizer(s_app.db_path);
    s_app.enroll_requested = false;

    ESP_LOGI(TAG, "Enrolled at boot: %d faces", s_app.recognizer->get_num_feats());
    std::string enroll_dir = std::string(CONFIG_BSP_SD_MOUNT_POINT) + "/enroll";
    enroll_jpegs_from_dir(&s_app, enroll_dir.c_str());

    xTaskCreate(stdin_task, "stdin_cmd", 4096, &s_app, 3, NULL);

    gpio_set_direction(BOARD_BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOARD_BOOT_BUTTON_GPIO, GPIO_PULLUP_ONLY);

    enum class UiState { NONE, DETECTED, RECOGNIZED };
    UiState ui_state = UiState::NONE;
    int last_id = -1;

    while (true) {
        poll_boot_button(&s_app);

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "Camera frame failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        dl::image::img_t img = frame_to_img(fb);
        UiState next_state = UiState::NONE;
        int matched_id = -1;
        float matched_sim = 0.0f;

        xSemaphoreTake(s_app.lock, portMAX_DELAY);
        auto &faces = s_app.detector->run(img);

        if (!faces.empty()) {
            if (s_app.enroll_requested) {
                s_app.enroll_requested = false;
                if (s_app.recognizer->enroll(img, faces) == ESP_OK) {
                    ESP_LOGI(TAG, "Live enroll OK, total: %d", s_app.recognizer->get_num_feats());
                } else {
                    ESP_LOGW(TAG, "Live enroll failed");
                }
            }

            auto matches = s_app.recognizer->recognize(img, faces);
            if (!matches.empty()) {
                next_state = UiState::RECOGNIZED;
                matched_id = matches.front().id;
                matched_sim = matches.front().similarity;
            } else {
                next_state = UiState::DETECTED;
            }
        }
        xSemaphoreGive(s_app.lock);

        esp_camera_fb_return(fb);

        switch (next_state) {
        case UiState::RECOGNIZED:
            board_status_led_set(BOARD_LED_GREEN);
            break;
        case UiState::DETECTED:
            board_status_led_set(BOARD_LED_YELLOW);
            break;
        default:
            board_status_led_set(BOARD_LED_OFF);
            break;
        }

        if (next_state != ui_state || (next_state == UiState::RECOGNIZED && matched_id != last_id)) {
            if (next_state == UiState::RECOGNIZED) {
                ESP_LOGI(TAG, "Recognized id=%d sim=%.3f", matched_id, matched_sim);
            } else if (next_state == UiState::DETECTED) {
                ESP_LOGI(TAG, "Face detected (unknown)");
            } else if (ui_state != UiState::NONE) {
                ESP_LOGI(TAG, "No face");
            }
            ui_state = next_state;
            last_id = matched_id;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return ESP_OK;
}
