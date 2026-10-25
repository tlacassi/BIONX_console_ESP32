/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#include "board_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#if defined(CONFIG_BIONX_DASHBOARD_DEMO) || defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)
static bool ready;
static SemaphoreHandle_t bus_mutex;
#ifdef CONFIG_BIONX_MCP_DEDICATED_SPI
static SemaphoreHandle_t mcp_bus_mutex;
void board_mcp_spi_lock(void) { xSemaphoreTakeRecursive(mcp_bus_mutex,portMAX_DELAY); }
void board_mcp_spi_unlock(void) { xSemaphoreGiveRecursive(mcp_bus_mutex); }
#else
void board_mcp_spi_lock(void) { board_spi_lock(); }
void board_mcp_spi_unlock(void) { board_spi_unlock(); }
#endif

void board_spi_lock(void) { xSemaphoreTakeRecursive(bus_mutex, portMAX_DELAY); }
void board_spi_unlock(void) { xSemaphoreGiveRecursive(bus_mutex); }

esp_err_t board_spi_init(void)
{
    if (ready) return ESP_OK;
    const int pins[] = {CONFIG_BIONX_LCD_RST, CONFIG_BIONX_LCD_CS, CONFIG_BIONX_LCD_DC,
        CONFIG_BIONX_LCD_MOSI, CONFIG_BIONX_LCD_SCLK,
#if defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)
        CONFIG_BIONX_MCP_MISO, CONFIG_BIONX_MCP_CS, CONFIG_BIONX_MCP_INT,
#ifdef CONFIG_BIONX_MCP_DEDICATED_SPI
        CONFIG_BIONX_MCP_MOSI, CONFIG_BIONX_MCP_SCLK,
#endif
#endif
    };
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        const int p = pins[i];
#ifdef CONFIG_BIONX_BOARD_HUZZAH32
        const bool input_only = i == 5 || i == 7; /* MCP MISO/INT after five LCD pins. */
        if (!GPIO_IS_VALID_GPIO(p) || (!input_only && !GPIO_IS_VALID_OUTPUT_GPIO(p)) ||
            p==0 || p==1 || p==2 || p==3 || p==12 || p==15 || (p>=6 && p<=11) || p==35) {
#else
        if (!(p == 0 || p == 1 || p == 3 || p == 4 || p == 5 || p == 6 || p == 7 || p == 10)) {
#endif
            ESP_LOGE("board_spi", "GPIO %d reserved or unsupported", p);
            return ESP_ERR_INVALID_ARG;
        }
#ifdef CONFIG_BIONX_ENCODER_JOG
        if(p==25 || p==26) {
            ESP_LOGE("board_spi","GPIO%d reserved for jog encoder",p);
            return ESP_ERR_INVALID_ARG;
        }
#endif
#ifdef CONFIG_BIONX_ASSIST_BRAKE
        if (p==36) {
            ESP_LOGE("board_spi","GPIO36 reserved for NC brake");
            return ESP_ERR_INVALID_ARG;
        }
#endif
#ifdef CONFIG_BIONX_MASTER_ENCODER_BUTTON
        if (p==22) {
            ESP_LOGE("board_spi","GPIO22 reserved for master button");
            return ESP_ERR_INVALID_ARG;
        }
#endif
#ifdef CONFIG_BIONX_COMPONENT_TEST
        if (p==25 || p==26 || p==CONFIG_BIONX_TEST_BUTTON_GPIO) {
            ESP_LOGE("board_spi","GPIO %d conflicts with encoder",p);
            return ESP_ERR_INVALID_ARG;
        }
#endif
        for (size_t j = 0; j < i; ++j) if (pins[j] == p) {
            ESP_LOGE("board_spi", "GPIO %d assigned twice", p);
            return ESP_ERR_INVALID_ARG;
        }
    }
    /* Inactive CS before the first clock. External CS pull-ups remain advised. */
    esp_err_t err = gpio_set_level(CONFIG_BIONX_LCD_CS, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_direction(CONFIG_BIONX_LCD_CS, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) return err;
#if defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)
    err = gpio_set_level(CONFIG_BIONX_MCP_CS, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_direction(CONFIG_BIONX_MCP_CS, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) return err;
#endif
    const spi_bus_config_t bus = {
        .mosi_io_num = CONFIG_BIONX_LCD_MOSI,
        .sclk_io_num = CONFIG_BIONX_LCD_SCLK,
#if (defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)) && !defined(CONFIG_BIONX_MCP_DEDICATED_SPI)
        .miso_io_num = CONFIG_BIONX_MCP_MISO,
#else
        .miso_io_num = -1,
#endif
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 240 * BOARD_LCD_LINES * sizeof(uint16_t),
    };
    err = spi_bus_initialize(BOARD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err!=ESP_OK) return err;
    bus_mutex=xSemaphoreCreateRecursiveMutex();
    if (!bus_mutex) { spi_bus_free(BOARD_SPI_HOST); return ESP_ERR_NO_MEM; }
#ifdef CONFIG_BIONX_MCP_DEDICATED_SPI
    const spi_bus_config_t mcp_bus={.mosi_io_num=CONFIG_BIONX_MCP_MOSI,
        .sclk_io_num=CONFIG_BIONX_MCP_SCLK,.miso_io_num=CONFIG_BIONX_MCP_MISO,
        .quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=32};
    err=spi_bus_initialize(BOARD_MCP_SPI_HOST,&mcp_bus,SPI_DMA_CH_AUTO);
    if (err!=ESP_OK) goto fail_lcd_bus;
    mcp_bus_mutex=xSemaphoreCreateRecursiveMutex();
    if (!mcp_bus_mutex) {
        err=ESP_ERR_NO_MEM;
        spi_bus_free(BOARD_MCP_SPI_HOST);
        goto fail_lcd_bus;
    }
    ESP_LOGI("board_spi","LCD SPI2 MOSI=%d SCK=%d; MCP SPI3 SI=%d SCK=%d SO=%d",
        CONFIG_BIONX_LCD_MOSI,CONFIG_BIONX_LCD_SCLK,CONFIG_BIONX_MCP_MOSI,
        CONFIG_BIONX_MCP_SCLK,CONFIG_BIONX_MCP_MISO);
#endif
    ready=true;
    return ESP_OK;
#ifdef CONFIG_BIONX_MCP_DEDICATED_SPI
fail_lcd_bus:
    vSemaphoreDelete(bus_mutex); bus_mutex=NULL;
    spi_bus_free(BOARD_SPI_HOST);
    return err;
#endif
}
#else
esp_err_t board_spi_init(void) { return ESP_ERR_NOT_SUPPORTED; }
void board_spi_lock(void) {}
void board_spi_unlock(void) {}
void board_mcp_spi_lock(void) {}
void board_mcp_spi_unlock(void) {}
#endif
