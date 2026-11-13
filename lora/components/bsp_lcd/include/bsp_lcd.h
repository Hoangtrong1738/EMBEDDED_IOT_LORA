#pragma once
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Khoi tao SPI2 + DMA, panel ILI9341, LVGL port (task + tick + double buffer DMA). */
esp_err_t bsp_lcd_init(void);

void bsp_lcd_backlight(bool on);

#ifdef __cplusplus
}
#endif
