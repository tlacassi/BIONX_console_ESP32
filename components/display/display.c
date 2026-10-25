/* SPDX-License-Identifier: GPL-3.0-only */
#include "display.h"
#include "sdkconfig.h"
#if defined(CONFIG_BIONX_DASHBOARD_DEMO) || defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)
#include "board_spi.h"
#include "shared_lcd_io.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lvgl_port.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "display";
static bool initialized;

esp_err_t display_init(void)
{
    if (initialized) return ESP_ERR_INVALID_STATE;
    esp_err_t err = board_spi_init();
    if (err != ESP_OK) return err;
    bool port_ready = false;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    lv_display_t *display = NULL;
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = CONFIG_BIONX_LCD_DC,
        .cs_gpio_num = CONFIG_BIONX_LCD_CS,
        .pclk_hz = CONFIG_BIONX_LCD_CLOCK_MHZ * 1000000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 1,
    };
#if defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)
    err = shared_lcd_io_new(&io_cfg, &io);
#else
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_SPI_HOST, &io_cfg, &io);
#endif
    if (err != ESP_OK) goto fail;
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CONFIG_BIONX_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_gc9a01(io, &panel_cfg, &panel);
    if (err != ESP_OK) goto fail;
    err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) goto fail;
    err = esp_lcd_panel_init(panel);
    if (err != ESP_OK) goto fail;
#ifdef CONFIG_BIONX_LCD_INVERT
    err = esp_lcd_panel_invert_color(panel, true);
    if (err != ESP_OK) goto fail;
#endif
    /* Finish panel commands before LVGL can start asynchronous SPI flushes.
     * esp_lcd's device handle must not be used concurrently by init and LVGL. */
    err = esp_lcd_panel_disp_on_off(panel, true);
    if (err != ESP_OK) goto fail;
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.task_stack = 6144;
    port_cfg.task_affinity = -1;
    port_cfg.task_max_sleep_ms = 20;
    err = lvgl_port_init(&port_cfg);
    if (err != ESP_OK) goto fail;
    port_ready = true;
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = 240 * BOARD_LCD_LINES,
        .double_buffer = true,
        .hres = 240,
        .vres = 240,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
#ifdef CONFIG_BIONX_LCD_SWAP_XY
            .swap_xy = true,
#endif
#ifdef CONFIG_BIONX_LCD_MIRROR_X
            .mirror_x = true,
#endif
#ifdef CONFIG_BIONX_LCD_MIRROR_Y
            .mirror_y = true,
#endif
        },
        .flags = {.buff_dma = true, .swap_bytes = true},
    };
    display = lvgl_port_add_disp(&disp_cfg);
    if (!display) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    initialized = true;
    ESP_LOGI(TAG, "GC9A01 240x240 RGB565, SPI %d MHz DMA, LVGL %d.%d.%d",
        CONFIG_BIONX_LCD_CLOCK_MHZ, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    ESP_LOGI(TAG, "DMA buffers=%u B; heap free=%u, minimum=%u, largest DMA block=%u",
        (unsigned)(2 * 240 * BOARD_LCD_LINES * sizeof(uint16_t)),
        (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "Display initialization failed: %s; heap=%u", esp_err_to_name(err),
        (unsigned)esp_get_free_heap_size());
    if (display) lvgl_port_remove_disp(display);
    if (port_ready) lvgl_port_deinit();
    if (panel) esp_lcd_panel_del(panel);
    if (io) esp_lcd_panel_io_del(io);
    return err;
}
#else
esp_err_t display_init(void) { return ESP_ERR_NOT_SUPPORTED; }
#endif
