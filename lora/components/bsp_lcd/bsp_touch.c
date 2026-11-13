#include "bsp_touch.h"
#include "board_config.h"

#include "esp_check.h"
#include "esp_log.h"

#if TOUCH_ENABLE
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_lvgl_port.h"
#endif

static const char *TAG = "bsp_touch";

esp_err_t bsp_touch_init(lv_display_t *disp)
{
#if !TOUCH_ENABLE
    (void)disp;
    ESP_LOGW(TAG, "XPT2046 dang tat: can khai bao chan T_DO/MISO truoc khi bat TOUCH_ENABLE");
    return ESP_OK;
#else
    if (LCD_PIN_MISO < 0) {
        ESP_LOGE(TAG, "TOUCH_ENABLE=1 nhung LCD_PIN_MISO chua hop le");
        return ESP_ERR_INVALID_STATE;
    }

    esp_lcd_panel_io_handle_t touch_io = NULL;
    const esp_lcd_panel_io_spi_config_t touch_io_cfg =
        ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(TOUCH_PIN_CS);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,
                                                  &touch_io_cfg, &touch_io), TAG,
                        "Khong tao duoc SPI IO cho XPT2046");

    /* ADC XPT2046 van duoc scale theo panel goc 240x320, sau do swap/mirror
     * truc tiep thanh toa do landscape 320x240 de khop voi LVGL rotation 0. */
    const esp_lcd_touch_config_t touch_cfg = {
        .x_max = LCD_PANEL_W,
        .y_max = LCD_PANEL_H,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = (gpio_num_t)TOUCH_PIN_IRQ,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = {
            .swap_xy = TOUCH_SWAP_XY,
            .mirror_x = TOUCH_MIRROR_X,
            .mirror_y = TOUCH_MIRROR_Y,
        },
    };
    esp_lcd_touch_handle_t touch = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_spi_xpt2046(touch_io, &touch_cfg, &touch), TAG,
                        "Khong tao duoc driver XPT2046");
#if TOUCH_PIN_IRQ >= 0
    gpio_pullup_en((gpio_num_t)TOUCH_PIN_IRQ);
#endif

    const lvgl_port_touch_cfg_t lvgl_touch_cfg = { .disp = disp, .handle = touch };
    if (lvgl_port_add_touch(&lvgl_touch_cfg) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "XPT2046 san sang: CS=%d IRQ=%d SPI2 chung voi TFT",
             TOUCH_PIN_CS, TOUCH_PIN_IRQ);
    return ESP_OK;
#endif
}
