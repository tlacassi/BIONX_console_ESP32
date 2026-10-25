/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#if defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)
#include <stdlib.h>
#include <string.h>
#include "shared_lcd_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "board_spi.h"
#include "driver/gpio.h"

/* Keep LCD and MCP on one transaction model. The stock LCD IO mixes queued
 * pixel DMA and polling commands; here every transfer finishes before return.
 * DMA-capable LVGL pixel buffers remain usable, with no queued SPI ISR. */
typedef struct {
    esp_lcd_panel_io_t base;
    spi_device_handle_t spi;
    int dc;
    esp_lcd_panel_io_callbacks_t callbacks;
    void *user;
} shared_io_t;

static esp_err_t send_bytes(shared_io_t *io, const void *data, size_t size, bool keep_cs)
{
    spi_transaction_t t = {.length=size*8, .tx_buffer=data,
        .flags=keep_cs ? SPI_TRANS_CS_KEEP_ACTIVE : 0};
    if (size<=4) {
        t.flags |= SPI_TRANS_USE_TXDATA;
        memcpy(t.tx_data,data,size);
    }
    return spi_device_polling_transmit(io->spi,&t);
}

static esp_err_t write_data(esp_lcd_panel_io_t *base, int cmd, const void *data, size_t size)
{
    if ((size && !data) || cmd>255 || cmd < -1) return ESP_ERR_INVALID_ARG;
    shared_io_t *io=(shared_io_t *)base;
    board_spi_lock();
    esp_err_t err=spi_device_acquire_bus(io->spi,portMAX_DELAY);
    if (err==ESP_OK) {
        if (cmd>=0) {
            uint8_t byte=(uint8_t)cmd;
            err=gpio_set_level(io->dc,0);
            if (err==ESP_OK) err=send_bytes(io,&byte,1,size!=0);
        }
        if (err==ESP_OK && size) {
            err=gpio_set_level(io->dc,1);
            if (err==ESP_OK) err=send_bytes(io,data,size,false);
        }
        spi_device_release_bus(io->spi);
    }
    board_spi_unlock();
    return err;
}

static esp_err_t write_color(esp_lcd_panel_io_t *base, int cmd, const void *data, size_t size)
{
    esp_err_t err=write_data(base,cmd,data,size);
    shared_io_t *io=(shared_io_t *)base;
    /* LVGL may reuse the buffer only after hardware completion. This callback
     * runs in task context, outside the SPI lock, not in an ISR. */
    if (err==ESP_OK && io->callbacks.on_color_trans_done) {
        esp_lcd_panel_io_event_data_t event={};
        io->callbacks.on_color_trans_done(base,&event,io->user);
    }
    return err;
}
static esp_err_t read_data(esp_lcd_panel_io_t *base,int cmd,void *data,size_t size)
{ (void)base; (void)cmd; (void)data; (void)size; return ESP_ERR_NOT_SUPPORTED; }
static esp_err_t callbacks(esp_lcd_panel_io_t *base,const esp_lcd_panel_io_callbacks_t *cb,void *user)
{
    if (!cb) return ESP_ERR_INVALID_ARG;
    shared_io_t *io=(shared_io_t *)base;
    io->callbacks=*cb; io->user=user; return ESP_OK;
}
static esp_err_t destroy(esp_lcd_panel_io_t *base)
{
    shared_io_t *io=(shared_io_t *)base;
    board_spi_lock();
    esp_err_t err=spi_bus_remove_device(io->spi);
    board_spi_unlock();
    if (err==ESP_OK) free(io);
    return err;
}
esp_err_t shared_lcd_io_new(const esp_lcd_panel_io_spi_config_t *cfg,esp_lcd_panel_io_handle_t *out)
{
    if (!cfg || !out || cfg->lcd_cmd_bits!=8 || cfg->lcd_param_bits!=8 ||
        cfg->dc_gpio_num<0 || cfg->spi_mode!=0) return ESP_ERR_INVALID_ARG;
    *out=NULL;
    shared_io_t *io=calloc(1,sizeof(*io));
    if (!io) return ESP_ERR_NO_MEM;
    io->dc=cfg->dc_gpio_num;
    io->base=(esp_lcd_panel_io_t){.tx_param=write_data,.tx_color=write_color,
        .rx_param=read_data,.register_event_callbacks=callbacks,.del=destroy};
    io->callbacks.on_color_trans_done=cfg->on_color_trans_done;
    io->user=cfg->user_ctx;
    const spi_device_interface_config_t device={.clock_speed_hz=cfg->pclk_hz,
        .mode=0,.spics_io_num=cfg->cs_gpio_num,.queue_size=1,.flags=SPI_DEVICE_HALFDUPLEX};
    esp_err_t err=spi_bus_add_device(BOARD_SPI_HOST,&device,&io->spi);
    if (err!=ESP_OK) { free(io); return err; }
    err=gpio_set_direction(io->dc,GPIO_MODE_OUTPUT);
    if (err!=ESP_OK) { spi_bus_remove_device(io->spi); free(io); return err; }
    *out=&io->base;
    return ESP_OK;
}
#endif
