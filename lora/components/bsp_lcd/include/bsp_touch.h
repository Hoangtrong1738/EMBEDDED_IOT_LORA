#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dang ky XPT2046 vao LVGL tren cung SPI2 voi ILI9341. */
esp_err_t bsp_touch_init(lv_display_t *disp);

#ifdef __cplusplus
}
#endif
