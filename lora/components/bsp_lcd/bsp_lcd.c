#include "bsp_lcd.h"
#include "board_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "bsp_lcd";

_Static_assert(LCD_H_RES == 320 && LCD_V_RES == 240,
               "Gateway UI must stay fixed at 320x240 landscape");
_Static_assert(LCD_LANDSCAPE_ROT == 270,
               "Gateway LCD rotation is intentionally locked at 270 degrees");

void bsp_lcd_backlight(bool on)
{
#if LCD_PIN_BL >= 0
    gpio_set_level((gpio_num_t)LCD_PIN_BL, on ? 1 : 0);
#else
    (void)on;
#endif
}

#if LCD_PERF_LOG
static int64_t s_t0;
static void refr_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_REFR_START) {
        s_t0 = esp_timer_get_time();
    } else {
        ESP_LOGI(TAG, "refresh %lld us", (long long)(esp_timer_get_time() - s_t0));
    }
}
#endif

/* Xoa toan bo GRAM theo toa do vat ly goc 240x320 cua ILI9341. */
static esp_err_t lcd_clear_gram(esp_lcd_panel_handle_t panel)
{
    const int lines = 20;
    const size_t bytes = LCD_PANEL_W * lines * sizeof(uint16_t);
    uint16_t *black = heap_caps_calloc(1, bytes, MALLOC_CAP_DMA);
    if (black == NULL) return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_OK;
    for (int y = 0; y < LCD_PANEL_H; y += lines) {
        err = esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_PANEL_W, y + lines,
                                        black);
        if (err != ESP_OK) break;
    }
    /* Lenh tham so cho den khi DMA mau truoc do xong, sau do moi giai phong buffer. */
    if (err == ESP_OK) {
        err = esp_lcd_panel_mirror(panel, LCD_MIRROR_X, LCD_MIRROR_Y);
    }
    free(black);
    return err;
}

esp_err_t bsp_lcd_init(void)
{
#if LCD_PIN_BL >= 0
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bl), TAG,
                        "Khong cau hinh duoc den nen LCD");
    bsp_lcd_backlight(false);          /* bat sau khi panel san sang, tranh nhap nhay */
#endif

#if TOUCH_PIN_CS >= 0
    /* Touch dang bi loai khoi firmware. Van giu CS XPT2046 o HIGH de chip
     * khong nghe xung SCLK/MOSI cua TFT va khong lai T_DO tren bus. */
    gpio_config_t touch_cs = {
        .pin_bit_mask = 1ULL << TOUCH_PIN_CS,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&touch_cs), TAG,
                        "Khong cau hinh duoc CS cam ung");
    ESP_RETURN_ON_ERROR(gpio_set_level((gpio_num_t)TOUCH_PIN_CS, 1), TAG,
                        "Khong keo duoc CS cam ung len HIGH");
#endif

    /* 1) Bus SPI2 + DMA. Moi giao dich toi da la mot dai ngang 24 dong. */
    const spi_bus_config_t bus = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_BUF_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus,
                                           SPI_DMA_CH_AUTO), TAG,
                        "Khong khoi tao duoc SPI2 cho LCD");

    /* 2) Panel IO (queue de DMA noi duoi nhieu lenh) */
    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_SPI_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,
                                 &io_cfg, &io),
        TAG, "Khong tao duoc panel IO cho LCD");

    /* 3) Driver ILI9341 */
    esp_lcd_panel_handle_t panel = NULL;
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_BGR_ORDER ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(io, &panel_cfg, &panel),
                        TAG, "Khong tao duoc driver ILI9341");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG,
                        "Khong reset duoc ILI9341");
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG,
                        "Khong khoi tao duoc ILI9341");
    /* Dat huong goc; lv_display_set_rotation() se lap MADCTL cho huong ngang. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel, LCD_SWAP_XY), TAG,
                        "Khong dat duoc swap XY cho LCD");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_mirror(panel, LCD_MIRROR_X, LCD_MIRROR_Y), TAG,
        "Khong dat duoc huong mirror cho LCD");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel, LCD_INVERT_COLOR),
                        TAG, "Khong dat duoc che do mau LCD");
    ESP_RETURN_ON_ERROR(lcd_clear_gram(panel), TAG,
                        "Khong xoa duoc GRAM cua LCD");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG,
                        "Khong bat duoc ILI9341");

    /* 4) LVGL port: task rieng, uu tien thap, ghim core 1 */
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_priority = LVGL_TASK_PRIO;
    lvgl_cfg.task_affinity = LVGL_TASK_CORE;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG,
                        "Khong khoi tao duoc LVGL port");

    /* 5) Hai dai DMA trong SRAM + buffer xoay 270 do sang khung 240x320 vat ly. */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * LCD_BUF_LINES,
        .double_buffer = true,
        .hres = LCD_PANEL_W,
        .vres = LCD_PANEL_H,
        .monochrome = false,
        .rotation = { .swap_xy = LCD_SWAP_XY, .mirror_x = LCD_MIRROR_X, .mirror_y = LCD_MIRROR_Y },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .swap_bytes = true,
            .full_refresh = false,
            .sw_rotate = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Khong cap duoc hai DMA strip cho LCD");
        return ESP_ERR_NO_MEM;
    }

    /* Khoa huong ngang: khong co API nao trong ung dung thay doi rotation nua. */
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Khong khoa duoc LVGL de dat huong man hinh");
        return ESP_ERR_TIMEOUT;
    }
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    lvgl_port_unlock();

    if (lv_display_get_horizontal_resolution(disp) != LCD_H_RES ||
        lv_display_get_vertical_resolution(disp) != LCD_V_RES) {
        ESP_LOGE(TAG, "Sai kich thuoc LVGL: %ldx%ld (can %dx%d)",
                 (long)lv_display_get_horizontal_resolution(disp),
                 (long)lv_display_get_vertical_resolution(disp),
                 LCD_H_RES, LCD_V_RES);
        return ESP_ERR_INVALID_SIZE;
    }

#if LCD_PERF_LOG
    lv_display_add_event_cb(disp, refr_cb, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(disp, refr_cb, LV_EVENT_REFR_READY, NULL);
#endif

    bsp_lcd_backlight(true);
    ESP_LOGI(TAG, "LCD khoa ngang: %dx%d, HW rot %d, SPI %d MHz, DMA strip %u byte x2",
             LCD_H_RES, LCD_V_RES, LCD_LANDSCAPE_ROT,
             LCD_SPI_HZ / 1000000,
             (unsigned)(LCD_H_RES * LCD_BUF_LINES * sizeof(uint16_t)));
    return ESP_OK;
}
