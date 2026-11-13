#pragma once
/* Tat ca chan va thong so phan cung o day. Doi day / doi man hinh chi sua file nay. */

/* SX127x DIO0. Neu phan cung dung chan khac, chi sua dong nay. */
#define LORA_DIO0_PIN       16

/* Product LEDs are active high. */
#define GATEWAY_LED_SYSTEM_PIN   42
/* GPIO35/36 are valid only on the confirmed non-Octal-PSRAM board variant. */
#define GATEWAY_LED_LINK_PIN     35
#define GATEWAY_LED_ALARM_PIN    36
#define GATEWAY_LED_ACTIVE_LEVEL  1
#define GATEWAY_LED_IDLE_LEVEL    0
#define GATEWAY_LED_BLINK_MS     400U

#define GATEWAY_BUZZER_PIN       38
#define GATEWAY_UART_TX_PIN      43
#define GATEWAY_UART_RX_PIN      44
#define GATEWAY_UART_BAUD_RATE   115200U

/* ---- Man hinh ILI9341 tren SPI2 ---- */
#define LCD_SPI_HOST        SPI2_HOST
#define LCD_PIN_SCLK        12
#define LCD_PIN_MOSI        11
#define LCD_PIN_CS          10
#define LCD_PIN_DC          14
#define LCD_PIN_RST         9
#define LCD_PIN_BL          8      /* dat -1 neu noi den nen thang 3V3 */
/* TFT ILI9341 chi ghi. Dat -1 de tach T_DO cua XPT2046 khoi SPI2. */
#define LCD_PIN_MISO        -1

/* Giao dien va LVGL chay truc tiep NGANG 320x240. ILI9341 duoc xoay bang
 * MADCTL mot lan khi khoi dong; khong xoay buffer DMA bang phan mem. */
#define LCD_H_RES           320
#define LCD_V_RES           240
/* ILI9341 has a native portrait GRAM of 240x320; LVGL rotates it to
 * the 320x240 gateway UI below. */
#define LCD_PANEL_W         240
#define LCD_PANEL_H         320
#define LCD_LANDSCAPE_ROT   270
/* Day breadboard dai: 4 MHz uu tien on dinh hon toc do. */
#define LCD_SPI_HZ          (4 * 1000 * 1000)

/* DMA: moi buffer = LCD_H_RES * LCD_BUF_LINES pixel (24 dong = 1/10 man hinh = 15 KB) */
#define LCD_BUF_LINES       24

#define LCD_INVERT_COLOR    0      /* ILI9341 thuong khong dao mau (dat 1 neu bi am ban) */
#define LCD_BGR_ORDER       1
/* Landscape voi day chan 14-pin cua module nam ben trai (xoay phan mem 270 do). */
#define LCD_SWAP_XY         0
#define LCD_MIRROR_X        0
#define LCD_MIRROR_Y        0

/* ---- Do thoi gian ve de kiem tra DMA (tat = 0 khi chay that) ---- */
#define LCD_PERF_LOG        0

/* LVGL's driver task still runs even though the gateway screen UI is disabled. */
#define LVGL_TASK_PRIO      2
#define LVGL_TASK_CORE      1

/* ---- Cam ung dien tro XPT2046, dung chung bus SPI2 cua ILI9341 ---- */
#define TOUCH_PIN_CS        21
#define TOUCH_PIN_IRQ       40
#define TOUCH_ENABLE        0
#define TOUCH_SWAP_XY       1
#define TOUCH_MIRROR_X      0
#define TOUCH_MIRROR_Y      1
