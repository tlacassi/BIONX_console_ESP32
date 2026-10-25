/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#include "can_observer.h"
#ifdef CONFIG_BIONX_CAN_OBSERVE
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "display.h"
#include "mcp2515_transport.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

static lv_obj_t *status_label, *counters;
static char counter_text[192];
static uint32_t log_suppressed;

static lv_obj_t *make_label(const char *text, int y, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text_static(label, text);
    lv_obj_set_width(label, 190);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static esp_err_t screen_init(void)
{
    if (!lvgl_port_lock(1000)) return ESP_ERR_TIMEOUT;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080e12), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    make_label("CAN PASSIVE", 35, &lv_font_montserrat_16, 0x41dcf5);
    status_label = make_label("INITIALIZING", 68, &lv_font_montserrat_16, 0xffb65c);
    counters = make_label("Motor / battery\ntelemetry: --", 100, &lv_font_montserrat_12, 0xf4fbff);
    make_label("NO MOTOR COMMANDS", 210, &lv_font_montserrat_12, 0x93a8b3);
    lvgl_port_unlock();
    return ESP_OK;
}

static void screen_status(const char *text)
{
    if (!lvgl_port_lock(20)) return;
    lv_label_set_text_static(status_label, text);
    lvgl_port_unlock();
}

static void report(const mcp2515_transport_stats_t *s, int64_t now)
{
    const int64_t age_ms = s->last_rx_us ? (now - s->last_rx_us) / 1000 : -1;
    ESP_LOGI("capture", "rx=%"PRIu32" malformed=%"PRIu32" queue_drop=%"PRIu32
        " ov0=%"PRIu32" ov1=%"PRIu32" log_omitted=%"PRIu32" spi_err=%"PRIu32
        " mode_fault=%"PRIu32" EFLG=%02x TEC=%u REC=%u age_ms=%"PRId64" fault=%s",
        s->chip.received, s->chip.malformed, s->capture_dropped,
        s->chip.overflow0_events, s->chip.overflow1_events, log_suppressed,
        s->chip.spi_errors, s->chip.mode_faults, s->chip.eflg, s->chip.tec,
        s->chip.rec, age_ms, esp_err_to_name(s->fault));
    if (!lvgl_port_lock(20)) return;
    lv_label_set_text_static(status_label, s->fault ? "RX FAULT" :
        !s->last_rx_us ? "WAITING FOR BUS" : age_ms > 1000 ? "BUS SILENT" : "RECEIVING");
    snprintf(counter_text, sizeof(counter_text),
        "RX: %"PRIu32"\nQueue drops: %"PRIu32"\nOverflow events: %"PRIu32
        "\nLog omitted: %"PRIu32"\nTelemetry: --",
        s->chip.received, s->capture_dropped,
        s->chip.overflow0_events + s->chip.overflow1_events, log_suppressed);
    lv_label_set_text_static(counters, counter_text);
    lvgl_port_unlock();
}

static void log_frame(const can_frame_t *f)
{
    /* One bounded line per selected frame; console output can still block.
     * Higher priority RX never waits for this consumer or for LVGL. */
    char line[256];
    int n = snprintf(line, sizeof(line), "CAN %"PRId64" ID=%08"PRIX32" %s %s DLC=%u",
        f->timestamp_us, f->id, f->extended ? "EXT" : "STD", f->rtr ? "RTR" : "DATA", f->dlc);
    if (!f->rtr) for (unsigned i = 0; i < f->dlc; ++i) {
        n += snprintf(line + n, sizeof(line) - n, " %02X", f->data[i]);
    }
    bionx_packet_t p;
    if (bionx_parse(f, &p)) {
        snprintf(line + n, sizeof(line) - n, " %s reg=%02X raw=%04X%s",
            p.kind == BIONX_REPLY ? "REPLY" : p.kind == BIONX_QUERY ? "QUERY" : p.kind == BIONX_SET ? "SET" : "UNKNOWN",
            p.reg, p.raw, p.kind == BIONX_REPLY ? " origin=unknown" : "");
        /* Only target-addressed traffic has an established node here.
         * Do not infer the origin of shared-ID replies from the register. */
        const bionx_register_info_t *info = bionx_register_info(p.target, p.reg);
        if (info) {
            n = (int)strlen(line);
            float value;
            const char *unit;
            if (bionx_value_candidate(&p, p.target, &value, &unit))
                snprintf(line + n, sizeof(line) - n, " %s candidate=%.3f%s", info->name, (double)value, unit);
            else snprintf(line + n, sizeof(line) - n, " %s", info->name);
        }
    }
    puts(line);
}

void can_observer_run(void)
{
    ESP_LOGI("observer", "Passive capture only; no CAN TX, battery wake or motor commands");
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(screen_init());
    esp_err_t err = mcp2515_transport_start();
    if (err != ESP_OK) {
        screen_status("CONFIG / SPI ERROR");
        ESP_LOGE("observer", "CAN disabled: %s. Verify hardware and oscillator in menuconfig.", esp_err_to_name(err));
        /* No repeated SPI init or busy loop with absent/unconfigured hardware. */
        return;
    }
    int64_t next_report = esp_timer_get_time() + 1000000;
    int64_t window = esp_timer_get_time() / 1000000;
    unsigned logged = 0;
    for (;;) {
        can_frame_t f;
        const bool got = mcp2515_capture_receive(&f, pdMS_TO_TICKS(10));
        const int64_t now = esp_timer_get_time();
        if (now / 1000000 != window) { window = now / 1000000; logged = 0; }
        if (got) {
            if (logged < CONFIG_BIONX_CAPTURE_LOG_RATE) { log_frame(&f); ++logged; }
            else ++log_suppressed;
        }
        if (now >= next_report) {
            mcp2515_transport_stats_t s;
            if (mcp2515_transport_snapshot(&s) == ESP_OK) report(&s, esp_timer_get_time());
            next_report = now + 1000000;
        }
    }
}
#else
void can_observer_run(void) {}
#endif
