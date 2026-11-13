idf_component_register(SRCS "bsp_lcd.c"
                       INCLUDE_DIRS "include"
                       REQUIRES board_config
                       PRIV_REQUIRES esp_lcd esp_driver_gpio esp_driver_spi
                                     esp_timer log lvgl esp_lvgl_port
                                     esp_lcd_ili9341)
