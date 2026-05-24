#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOARD_LED_OFF = 0,
    BOARD_LED_YELLOW,
    BOARD_LED_GREEN,
} board_led_color_t;

esp_err_t board_status_led_init(void);
void board_status_led_set(board_led_color_t color);

#ifdef __cplusplus
}
#endif
