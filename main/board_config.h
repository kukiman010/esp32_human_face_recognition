#pragma once

/**
 * ESP32-S3-N16R8-CAM (OV3660 + SD_MMC 1-bit).
 * DVP/SD pins match esp32_s3_eye BSP; SCCB and XCLK differ from BSP defaults.
 * Source of truth: cam_web_test/include/camera.h, sd_card.h
 */

#include "driver/gpio.h"
#include "esp_camera.h"

#define BOARD_NAME "ESP32-S3-N16R8-CAM"

/* Camera DVP */
#define BOARD_CAM_PWDN   GPIO_NUM_NC
#define BOARD_CAM_RESET  GPIO_NUM_NC
#define BOARD_CAM_XCLK   GPIO_NUM_15
#define BOARD_CAM_PCLK   GPIO_NUM_13
#define BOARD_CAM_VSYNC  GPIO_NUM_6
#define BOARD_CAM_HREF   GPIO_NUM_7
#define BOARD_CAM_D0     GPIO_NUM_11
#define BOARD_CAM_D1     GPIO_NUM_9
#define BOARD_CAM_D2     GPIO_NUM_8
#define BOARD_CAM_D3     GPIO_NUM_10
#define BOARD_CAM_D4     GPIO_NUM_12
#define BOARD_CAM_D5     GPIO_NUM_18
#define BOARD_CAM_D6     GPIO_NUM_17
#define BOARD_CAM_D7     GPIO_NUM_16

/* OV3660 SCCB — dedicated pins (not shared BSP I2C) */
#define BOARD_CAM_SCCB_SDA GPIO_NUM_4
#define BOARD_CAM_SCCB_SCL GPIO_NUM_5

/* SD_MMC 1-bit — same as BSP esp32_s3_eye */
#define BOARD_SD_CLK GPIO_NUM_39
#define BOARD_SD_CMD GPIO_NUM_38
#define BOARD_SD_D0  GPIO_NUM_40

/* WS2812 status LED (yellow = face, green = recognized) */
#define BOARD_STATUS_LED_GPIO GPIO_NUM_48

/** BOOT button — short press enrolls next detected face. */
#define BOARD_BOOT_BUTTON_GPIO GPIO_NUM_0

/** Settings verified on this board in cam_web_test (Arduino). */
#define BOARD_CAM_XCLK_HZ 8000000

/**
 * Face ML pipeline: RGB565 QVGA in PSRAM.
 * Use board_camera_init() instead of BSP_CAMERA_DEFAULT_CONFIG.
 */
#define BOARD_CAMERA_CONFIG                                         \
    {                                                               \
        .pin_pwdn = BOARD_CAM_PWDN,                                 \
        .pin_reset = BOARD_CAM_RESET,                               \
        .pin_xclk = BOARD_CAM_XCLK,                                 \
        .pin_sccb_sda = BOARD_CAM_SCCB_SDA,                         \
        .pin_sccb_scl = BOARD_CAM_SCCB_SCL,                         \
        .pin_d7 = BOARD_CAM_D7,                                     \
        .pin_d6 = BOARD_CAM_D6,                                     \
        .pin_d5 = BOARD_CAM_D5,                                     \
        .pin_d4 = BOARD_CAM_D4,                                     \
        .pin_d3 = BOARD_CAM_D3,                                     \
        .pin_d2 = BOARD_CAM_D2,                                     \
        .pin_d1 = BOARD_CAM_D1,                                     \
        .pin_d0 = BOARD_CAM_D0,                                     \
        .pin_vsync = BOARD_CAM_VSYNC,                               \
        .pin_href = BOARD_CAM_HREF,                                 \
        .pin_pclk = BOARD_CAM_PCLK,                                 \
        .xclk_freq_hz = BOARD_CAM_XCLK_HZ,                          \
        .ledc_timer = LEDC_TIMER_0,                                 \
        .ledc_channel = LEDC_CHANNEL_0,                             \
        .pixel_format = PIXFORMAT_RGB565,                           \
        .frame_size = FRAMESIZE_QVGA,                               \
        .jpeg_quality = 12,                                         \
        .fb_count = 2,                                              \
        .fb_location = CAMERA_FB_IN_PSRAM,                          \
        .grab_mode = CAMERA_GRAB_LATEST,                            \
    }
