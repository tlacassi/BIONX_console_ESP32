/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#include <string.h>
#include <stdio.h>
#include "mcp2515_transport.h"
#if defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)
#include "board_spi.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static mcp2515_t chip;
static spi_device_handle_t spi;
static QueueHandle_t capture;
static StaticQueue_t capture_control;
static uint8_t capture_storage[64 * sizeof(can_frame_t)];
static SemaphoreHandle_t mutex;
static portMUX_TYPE irq_lock=portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t rx_task;
static esp_timer_handle_t rx_timer;
static void poll_wakeup(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&irq_lock);
    if (rx_task) xTaskNotifyGive(rx_task);
    portEXIT_CRITICAL(&irq_lock);
}
static uint32_t capture_dropped;
#ifdef CONFIG_BIONX_MCP_USE_INT
static volatile uint32_t irq_count;
#endif
static esp_err_t fault;
static int64_t last_rx_us;
static uint32_t rx_polls, max_poll_us;
static uint64_t rx_poll_us;

/* Diagnostic readers must never wait behind RX/TX chip ownership. */
static portMUX_TYPE stats_lock=portMUX_INITIALIZER_UNLOCKED;
static mcp2515_transport_stats_t published;
/* Caller owns chip mutex, or startup has not launched RX yet. */
static void publish_stats(void)
{
    mcp2515_transport_stats_t value={.chip=chip.stats,
        .capture_dropped=capture_dropped,.fault=fault,.last_rx_us=last_rx_us};
    value.rx_polls=rx_polls; value.rx_poll_us=rx_poll_us; value.max_poll_us=max_poll_us;
#ifdef CONFIG_BIONX_MCP_USE_INT
    value.irq_edges=irq_count;
#endif
    portENTER_CRITICAL(&stats_lock);
    published=value;
    portEXIT_CRITICAL(&stats_lock);
}

static esp_err_t spi_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    (void)ctx;
    /* Inline payloads bypass DMA bounce buffers for register/bit-modify IO. */
    spi_transaction_t t = {.length = len * 8};
    if (len <= 4) {
        t.flags = SPI_TRANS_USE_TXDATA;
        memcpy(t.tx_data, tx, len);
        if (rx) t.flags |= SPI_TRANS_USE_RXDATA;
    } else {
        t.tx_buffer = tx;
        t.rx_buffer = rx;
    }
    /* All shared-bus clients use synchronous polling under this task lock.
     * No queued SPI ISR can overlap a polling transaction. */
    board_mcp_spi_lock();
    esp_err_t err=spi_device_acquire_bus(spi,portMAX_DELAY);
    if (err==ESP_OK) {
        err=spi_device_polling_transmit(spi,&t);
        spi_device_release_bus(spi);
    }
    board_mcp_spi_unlock();
    if (err==ESP_OK && rx && len<=4) memcpy(rx,t.rx_data,len);
#ifdef CONFIG_BIONX_COMPONENT_TEST
    static unsigned probes;
    if (rx && len>=3 && tx[0]==0x03 && probes++<16)
        ESP_LOGD("mcp_probe","SPI READ reg=%02X value=%02X status=%s",tx[1],rx[2],esp_err_to_name(err));
#endif
    return err;
}
static void delay_ms(void *ctx, unsigned ms) { (void)ctx; vTaskDelay(pdMS_TO_TICKS(ms)); }
static int64_t now_us(void *ctx) { (void)ctx; return esp_timer_get_time(); }

#ifdef CONFIG_BIONX_MCP_USE_INT
static void interrupt(void *arg)
{
    (void)arg;
    BaseType_t wake = pdFALSE;
    portENTER_CRITICAL_ISR(&irq_lock);
    ++irq_count;
    if (rx_task) vTaskNotifyGiveFromISR(rx_task, &wake);
    portEXIT_CRITICAL_ISR(&irq_lock);
    if (wake) portYIELD_FROM_ISR();
}

#endif
static void receive_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* INT remains low until flags are serviced. Poll fallback also handles
         * an interrupt missed at startup; never rely only on a falling edge. */
        for (unsigned budget = 0; budget < 8; ++budget) {
            can_frame_t frame;
            xSemaphoreTake(mutex, portMAX_DELAY);
            int64_t poll_start=esp_timer_get_time();
            /* Keep flags/payload/ack together: do not let a pixel transfer
             * interleave between every register access of the same frame. */
            board_mcp_spi_lock();
            esp_err_t err = mcp2515_poll_fast(&chip, &frame);
            board_mcp_spi_unlock();
            uint32_t elapsed=(uint32_t)(esp_timer_get_time()-poll_start);
            ++rx_polls; rx_poll_us+=elapsed; if (elapsed>max_poll_us) max_poll_us=elapsed;
            if (err == ESP_OK) {
                last_rx_us = frame.timestamp_us;
                if (xQueueSend(capture, &frame, 0) != pdTRUE) ++capture_dropped;
            } else if (err != ESP_ERR_NOT_FOUND && err != ESP_ERR_INVALID_RESPONSE) {
                fault = err;
                if (chip.active) mcp2515_tx_abort(&chip);
            }
            const bool failed = fault != ESP_OK;
            publish_stats();
            xSemaphoreGive(mutex);
            if (failed) {
                esp_timer_stop(rx_timer);
                gpio_intr_disable(CONFIG_BIONX_MCP_INT);
                portENTER_CRITICAL(&irq_lock);
                rx_task = NULL; /* ISR must not notify a deleted task, on either CPU. */
                portEXIT_CRITICAL(&irq_lock);
                ESP_LOGE("mcp2515", "RX latched off: %s; manual restart required", esp_err_to_name(fault));
                vTaskDelete(NULL);
            }
            if (err == ESP_ERR_NOT_FOUND) break;
        }
        /* Bound CPU ownership even if INT is stuck low or traffic saturates. */
        /* A tick-wide sleep can miss a burst filling both hardware buffers.
         * IRQ or timer notification blocks the task instead of busy-waiting. */
        /* In active mode full health checks take longer than passive polling.
         * A timer notification may already be pending on every pass. Yield a
         * tick with no chip lock so the master can acquire it for TX/status.
         * Merely consuming pending notifications can starve TX on the other CPU. */
        if (chip.active) vTaskDelay(1);
        ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(2) ? pdMS_TO_TICKS(2) : 1);
    }
}

static esp_err_t start(bool active)
{
    if (capture) return ESP_ERR_INVALID_STATE;
#ifndef CONFIG_BIONX_MCP_HARDWARE_VERIFIED
    ESP_LOGE("mcp2515", "Disabled: verify module levels/supply and enable hardware option first");
    return ESP_ERR_INVALID_STATE;
#endif
    const unsigned osc = CONFIG_BIONX_MCP_OSC_MHZ;
    if (osc != 8 && osc != 16 && osc != 20) {
        ESP_LOGE("mcp2515", "Select verified oscillator 8, 16 or 20 MHz; no guessed default");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = board_spi_init();
    if (err != ESP_OK) return err;
    const spi_device_interface_config_t cfg = {
        .clock_speed_hz = CONFIG_BIONX_MCP_SPI_CLOCK_MHZ * 1000000, .mode = 0,
        .spics_io_num = CONFIG_BIONX_MCP_CS, .queue_size = 1,
    };
    err = spi_bus_add_device(BOARD_MCP_SPI_HOST, &cfg, &spi);
    if (err != ESP_OK) return err;
    const mcp2515_io_t io = {.transfer = spi_transfer, .delay_ms = delay_ms, .now_us = now_us};
    err = active ? mcp2515_init_active(&chip, &io, osc) : mcp2515_init_passive(&chip, &io, osc);
    if (err != ESP_OK) goto fail;
#ifdef CONFIG_BIONX_MCP_USE_INT
    const gpio_config_t irq_cfg = {.pin_bit_mask = 1ULL << CONFIG_BIONX_MCP_INT,
        .mode = GPIO_MODE_INPUT, .pull_up_en = CONFIG_BIONX_MCP_INT >= 34 ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_NEGEDGE};
    err = gpio_config(&irq_cfg);
    if (err != ESP_OK) goto fail;
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;
    err = gpio_isr_handler_add(CONFIG_BIONX_MCP_INT, interrupt, NULL);
    if (err != ESP_OK) goto fail;
#endif
    mutex = xSemaphoreCreateMutex();
    if (!mutex) { err = ESP_ERR_NO_MEM; goto fail_irq; }
    capture = xQueueCreateStatic(64, sizeof(can_frame_t), capture_storage, &capture_control);
    if (!capture) { err = ESP_ERR_NO_MEM; goto fail_mutex; }
    const esp_timer_create_args_t timer_cfg={.callback=poll_wakeup,.name="can_poll",
        .skip_unhandled_events=true};
    err=esp_timer_create(&timer_cfg,&rx_timer);
    if (err!=ESP_OK) {
        vQueueDelete(capture); capture=NULL;
        goto fail_mutex;
    }
    publish_stats();
    if (xTaskCreate(receive_task, "can_rx", 4096, NULL, 5, &rx_task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        vQueueDelete(capture); capture = NULL;
        esp_timer_delete(rx_timer); rx_timer=NULL;
        goto fail_mutex;
    }
#ifdef CONFIG_BIONX_MCP_USE_INT
    err=esp_timer_start_periodic(rx_timer,1000);
#else
    err=esp_timer_start_periodic(rx_timer,250);
#endif
    if (err!=ESP_OK) {
        /* Start cannot fail for the fresh timer and a supported period, but
         * never leave a partially started RX path alive if it does. */
        xSemaphoreTake(mutex,portMAX_DELAY);
        portENTER_CRITICAL(&irq_lock);
        TaskHandle_t task=rx_task; rx_task=NULL;
        portEXIT_CRITICAL(&irq_lock);
        if (task) vTaskDelete(task);
        publish_stats();
        xSemaphoreGive(mutex);
        esp_timer_delete(rx_timer); rx_timer=NULL;
        vQueueDelete(capture); capture=NULL;
        goto fail_mutex;
    }
    ESP_LOGI("mcp2515", "%s 125 kbit/s, osc=%u MHz, RX queue=64", active ? "NORMAL one-shot" : "LISTEN ONLY", osc);
    return ESP_OK;
fail_mutex:
    vSemaphoreDelete(mutex); mutex = NULL;
fail_irq:
    gpio_isr_handler_remove(CONFIG_BIONX_MCP_INT);
fail:
#if defined(CONFIG_BIONX_COMPONENT_TEST) && defined(CONFIG_BIONX_MCP_DEDICATED_SPI)
    /* Startup failure only: read-only routing dump, never in the RX hot path. */
    gpio_dump_io_configuration(stdout,(1ULL<<CONFIG_BIONX_LCD_MOSI) |
        (1ULL<<CONFIG_BIONX_LCD_SCLK) | (1ULL<<CONFIG_BIONX_MCP_MOSI) |
        (1ULL<<CONFIG_BIONX_MCP_SCLK) | (1ULL<<CONFIG_BIONX_MCP_MISO) |
        (1ULL<<CONFIG_BIONX_MCP_CS));
#endif
    spi_bus_remove_device(spi); spi = NULL;
    ESP_LOGE("mcp2515", "Initialization failed: %s; last mode=%02X SPI errors=%lu",
        esp_err_to_name(err),chip.stats.mode,(unsigned long)chip.stats.spi_errors);
    return err;
}

esp_err_t mcp2515_transport_start(void) { return start(false); }
#ifdef CONFIG_BIONX_CAN_MASTER
esp_err_t mcp2515_transport_start_master(void) { return start(true); }
#else
esp_err_t mcp2515_transport_start_master(void) { return ESP_ERR_NOT_SUPPORTED; }
#endif

static esp_err_t send_frame(const can_frame_t *frame, unsigned timeout_ms, bool probe)
{
    if (!capture || !mutex || !frame || !timeout_ms || timeout_ms>100) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(mutex,pdMS_TO_TICKS(20))!=pdTRUE) return ESP_ERR_TIMEOUT;
    chip.discovery=probe;
    esp_err_t err = fault ? fault : mcp2515_tx_begin(&chip,frame);
    publish_stats();
    xSemaphoreGive(mutex);
    if (err!=ESP_OK) return err;
    int64_t deadline=esp_timer_get_time()+timeout_ms*1000;
    do {
        if (xSemaphoreTake(mutex,pdMS_TO_TICKS(20))!=pdTRUE) break;
        err=fault ? fault : mcp2515_tx_status(&chip);
        if (err!=ESP_OK && err!=ESP_ERR_NOT_FOUND &&
            !(probe && err==ESP_ERR_INVALID_RESPONSE && chip.stats.ready)) {
            mcp2515_tx_abort(&chip);
            fault=err;
        }
        publish_stats();
        xSemaphoreGive(mutex);
        if (err!=ESP_ERR_NOT_FOUND) return err;
        vTaskDelay(1); /* RX can drain between TX status checks. */
    } while (esp_timer_get_time()<deadline);
    if (xSemaphoreTake(mutex,pdMS_TO_TICKS(20))==pdTRUE) {
        /* Scheduling may resume after the deadline even though the chip has
         * already completed TX. Inspect once more before declaring timeout;
         * this never retransmits the frame. */
        err=fault ? fault : mcp2515_tx_status(&chip);
        if (err==ESP_OK || (probe && err==ESP_ERR_INVALID_RESPONSE && chip.stats.ready)) {
            publish_stats();
            xSemaphoreGive(mutex);
            return err;
        }
        ++chip.stats.tx_timeouts;
        if (probe && chip.stats.ready && !fault) {
            esp_err_t cancel=mcp2515_probe_cancel(&chip);
            if (cancel!=ESP_OK) fault=cancel;
        } else {
            mcp2515_tx_abort(&chip);
            fault=ESP_ERR_TIMEOUT;
        }
        publish_stats();
        xSemaphoreGive(mutex);
    }
    ESP_LOGE("mcp2515","TX deadline id=%03lx reg=%02x status=%s",
        (unsigned long)frame->id,frame->data[1],esp_err_to_name(err));
    return ESP_ERR_TIMEOUT;
}

esp_err_t mcp2515_transport_send(const can_frame_t *frame, unsigned timeout_ms)
{ return send_frame(frame,timeout_ms,false); }

esp_err_t mcp2515_transport_probe(const can_frame_t *f, unsigned timeout_ms)
{
    if (!f || f->id!=0x10 || f->extended || f->rtr || f->dlc!=2 ||
        f->data[0]!=0 || f->data[1]!=0x3b) return ESP_ERR_INVALID_ARG;
    return send_frame(f,timeout_ms,true);
}

bool mcp2515_capture_receive(can_frame_t *out, TickType_t wait)
{
    return out && capture && xQueueReceive(capture, out, wait) == pdTRUE;
}

esp_err_t mcp2515_transport_snapshot(mcp2515_transport_stats_t *out)
{
    if (!out || !capture || !mutex) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&stats_lock);
    *out=published;
    portEXIT_CRITICAL(&stats_lock);
    out->queue_used=uxQueueMessagesWaiting(capture);
    return ESP_OK;
}
#else
esp_err_t mcp2515_transport_probe(const can_frame_t *f,unsigned ms) { (void)f; (void)ms; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t mcp2515_transport_start(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t mcp2515_transport_start_master(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t mcp2515_transport_send(const can_frame_t *f,unsigned ms) { (void)f; (void)ms; return ESP_ERR_NOT_SUPPORTED; }
bool mcp2515_capture_receive(can_frame_t *out, TickType_t wait) { (void)out; (void)wait; return false; }
esp_err_t mcp2515_transport_snapshot(mcp2515_transport_stats_t *out) { (void)out; return ESP_ERR_NOT_SUPPORTED; }
#endif
