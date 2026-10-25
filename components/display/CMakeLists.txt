idf_component_register(SRCS "display.c" "shared_lcd_io.c" INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_lcd_gc9a01 esp_lvgl_port driver board_spi)
