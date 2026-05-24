#include "board_status_led.h"

#include "board_config.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

static const char *TAG = "board_led";
static led_strip_handle_t s_strip = NULL;

esp_err_t board_status_led_init(void)
{
#if !CONFIG_BOARD_STATUS_LED_ENABLE
    return ESP_OK;
#endif

    led_strip_config_t strip_config = {
        .strip_gpio_num = BOARD_STATUS_LED_GPIO,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LED init failed: %s", esp_err_to_name(err));
        s_strip = NULL;
        return err;
    }

    board_status_led_set(BOARD_LED_OFF);
    ESP_LOGI(TAG, "WS2812 on GPIO %d ready", BOARD_STATUS_LED_GPIO);
    return ESP_OK;
}

void board_status_led_set(board_led_color_t color)
{
#if !CONFIG_BOARD_STATUS_LED_ENABLE
    (void)color;
    return;
#endif

    if (!s_strip) {
        return;
    }

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    switch (color) {
    case BOARD_LED_YELLOW:
        r = 255;
        g = 180;
        b = 0;
        break;
    case BOARD_LED_GREEN:
        g = 255;
        break;
    case BOARD_LED_OFF:
    default:
        break;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_set_pixel(s_strip, 0, r, g, b));
    ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_refresh(s_strip));
}
