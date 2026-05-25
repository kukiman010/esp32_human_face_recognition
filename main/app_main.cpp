#include "board_camera.h"
#include "board_config.h"
#include "board_status_led.h"
#include "board_wifi.h"
#include "face_app.h"
#include "face_preview.h"
#include "spiflash_fatfs.hpp"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <filesystem>

static const char *TAG = "human_face_recognition";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Board: %s — live face recognition", BOARD_NAME);

#if CONFIG_DB_FATFS_FLASH
    ESP_ERROR_CHECK(fatfs_flash_mount());
#endif
#if CONFIG_DB_SPIFFS
    ESP_ERROR_CHECK(bsp_spiffs_mount());
#endif
#if CONFIG_DB_FATFS_SDCARD || CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD || CONFIG_HUMAN_FACE_FEAT_MODEL_IN_SDCARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    ESP_LOGI(TAG, "SD mounted at %s (CLK=%d CMD=%d D0=%d)",
             CONFIG_BSP_SD_MOUNT_POINT, BOARD_SD_CLK, BOARD_SD_CMD, BOARD_SD_D0);
#endif

    ESP_ERROR_CHECK(board_camera_init());
    board_status_led_init();
    face_preview_init();

#if CONFIG_BOARD_WEB_ENABLE
    ESP_ERROR_CHECK(board_wifi_init());
#endif

#if CONFIG_DB_FATFS_FLASH
    auto db_path = std::filesystem::path(CONFIG_SPIFLASH_MOUNT_POINT) / "face.db";
#elif CONFIG_DB_SPIFFS
    auto db_path = std::filesystem::path(CONFIG_BSP_SPIFFS_MOUNT_POINT) / "face.db";
#else
    auto db_path = std::filesystem::path(CONFIG_BSP_SD_MOUNT_POINT) / "face.db";
#endif

    ESP_LOGI(TAG, "Database: %s", db_path.string().c_str());
    ESP_LOGI(TAG, "Type 'help' in monitor for commands");

    face_app_start(db_path.string().c_str());
}
