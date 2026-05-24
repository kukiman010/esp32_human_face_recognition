#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize OV3660 with BOARD_CAMERA_CONFIG (direct SCCB, 8 MHz XCLK). */
esp_err_t board_camera_init(void);

/** Deinitialize camera driver. */
esp_err_t board_camera_deinit(void);

#ifdef __cplusplus
}
#endif
