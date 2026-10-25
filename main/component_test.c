/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#include "component_test.h"
#ifdef CONFIG_BIONX_COMPONENT_TEST
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "display.h"
#include "board_spi.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "mcp2515_transport.h"
#include "freertos/task.h"

#ifdef CONFIG_BIONX_TEST_MCP
static const bool test_mcp=true;
#else
static const bool test_mcp=false;
#endif

void component_test_run(void)
{
    /* Sample above LVGL priority, with a one-tick yield between samples. */
    vTaskPrioritySet(NULL,3);
    if (CONFIG_BIONX_TEST_BUTTON_GPIO!=-1 && CONFIG_BIONX_TEST_BUTTON_GPIO!=22) {
        ESP_LOGE("hwtest","Test button must be disabled (-1) or on GPIO22"); return;
    }
    ESP_LOGI("hwtest","HUZZAH32 encoder test; MCP enabled=%d; no CAN TX",test_mcp);
#ifdef CONFIG_BIONX_MCP_SERIAL_ONLY
    const bool serial_only=true;
#else
    const bool serial_only=false;
#endif
    esp_err_t err=serial_only ? board_spi_init() : display_init();
    if (err!=ESP_OK) { ESP_LOGE("hwtest","Display init: %s",esp_err_to_name(err)); return; }
    gpio_config_t pins={.pin_bit_mask=(1ULL<<25)|(1ULL<<26),.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&pins));
#if CONFIG_BIONX_TEST_BUTTON_GPIO >= 0
    pins.pin_bit_mask=1ULL<<CONFIG_BIONX_TEST_BUTTON_GPIO;
    ESP_ERROR_CHECK(gpio_config(&pins));
#endif
    lv_obj_t *text=NULL;
    if (!serial_only && lvgl_port_lock(1000)) {
        lv_obj_t *screen=lv_screen_active();
        lv_obj_set_style_bg_color(screen,lv_color_hex(0x101820),0);
        lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
        const uint32_t colors[]={0xff0000,0x00ff00,0x0000ff};
        for(unsigned i=0;i<3;++i) {
            lv_obj_t *box=lv_obj_create(screen);
            lv_obj_set_size(box,36,16);
            lv_obj_set_pos(box,58+44*i,42);
            lv_obj_set_style_bg_color(box,lv_color_hex(colors[i]),0);
            lv_obj_set_style_bg_opa(box,LV_OPA_COVER,0);
            lv_obj_set_style_border_width(box,0,0);
        }
        text=lv_label_create(screen);
        lv_obj_set_width(text,208);
        lv_obj_set_pos(text,16,70);
        lv_obj_set_style_text_color(text,lv_color_hex(0xffffff),0);
        lv_obj_set_style_text_font(text,&lv_font_montserrat_12,0);
        lv_obj_set_style_text_align(text,LV_TEXT_ALIGN_CENTER,0);
        lv_label_set_text(text,"ENCODER TEST\nStarting...");
        lvgl_port_unlock();
    }
    const esp_err_t mcp=test_mcp ? mcp2515_transport_start() : ESP_ERR_NOT_SUPPORTED;
    if (test_mcp) {
#ifdef CONFIG_BIONX_MCP_USE_INT
        const char *rx_mode="INT + 1ms fallback";
#else
        const char *rx_mode="polling 250us";
#endif
        ESP_LOGI("hwtest","MCP init=%s; quartz=%d MHz; %s",
            esp_err_to_name(mcp),CONFIG_BIONX_MCP_OSC_MHZ,rx_mode);
    } else ESP_LOGI("hwtest","MCP disabled: no initialization, no CAN task");
    /* Gray-code accumulator: reversals/bounce cancel; illegal two-bit jumps
     * reset the partial step instead of inventing a turn. */
    const int8_t transitions[]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    unsigned prev=(gpio_get_level(25)<<1)|gpio_get_level(26);
    int partial=0; int32_t turns=0; uint32_t jumps=0, clicks=0;
    int button_raw=1, button_stable=1; int64_t button_since=0, next_report=0, next_ui=0, next_perf=0;
    int32_t ui_turns=0; uint32_t ui_clicks=0; int ui_button=1;
    for (;;) {
        int64_t now=esp_timer_get_time();
        unsigned ab=(gpio_get_level(25)<<1)|gpio_get_level(26);
        if ((prev^ab)==3) { ++jumps; partial=0; }
        else partial+=transitions[(prev<<2)|ab];
        prev=ab;
        if (partial>=4) { if (turns<INT32_MAX) ++turns; partial=0; }
        if (partial<=-4) { if (turns>INT32_MIN) --turns; partial=0; }
#if CONFIG_BIONX_TEST_BUTTON_GPIO >= 0
        int raw=gpio_get_level(CONFIG_BIONX_TEST_BUTTON_GPIO);
        if (raw!=button_raw) { button_raw=raw; button_since=now; }
        if (raw!=button_stable && now-button_since>=25000) {
            button_stable=raw;
            if (!raw) ++clicks;
        }
#else
        (void)button_raw; (void)button_since;
#endif
        /* Drain capture to measure queue behavior without serial per-frame load. */
        can_frame_t f;
        for(unsigned i=0;i<32 && mcp==ESP_OK && mcp2515_capture_receive(&f,0);++i) {
            static unsigned shown;
            if (shown<12) {
                char line[96];
                int n=snprintf(line,sizeof(line),"ID=%03lX DLC=%u%s",(unsigned long)f.id,f.dlc,f.rtr?" RTR":"");
                if (!f.rtr) for(unsigned b=0;b<f.dlc;++b) n+=snprintf(line+n,sizeof(line)-n," %02X",f.data[b]);
                ESP_LOGI("can_test","%s",line);
                ++shown;
            }
        }
        if (now>=next_report) {
            next_report=now+100000;
            mcp2515_transport_stats_t stats={0};
            esp_err_t status=mcp==ESP_OK ? mcp2515_transport_snapshot(&stats) : mcp;
            const char *state=!test_mcp ? "DISABLED" :
                mcp==ESP_OK && status==ESP_ERR_TIMEOUT ? "STATS BUSY" : status!=ESP_OK ? esp_err_to_name(status) : stats.fault ? "FAULT" :
                stats.chip.received ? (now-stats.last_rx_us>1000000 ? "RX IDLE" : "RX OK") : "SPI OK / waiting CAN";
            ESP_LOGI("hwtest","ENC=%ld AB=%u%u jumps=%lu button=%d clicks=%lu MCP=%s rx=%lu drops=%lu ov=%lu",
                (long)turns,(ab>>1)&1,ab&1,(unsigned long)jumps,button_stable,(unsigned long)clicks,state,
                (unsigned long)stats.chip.received,(unsigned long)stats.capture_dropped,
                (unsigned long)(stats.chip.overflow0_events+stats.chip.overflow1_events));
            if (test_mcp && status==ESP_OK && now>=next_perf) {
                next_perf=now+5000000;
                ESP_LOGI("can_perf","polls=%lu mean=%llu us max=%lu us INT=%lu bad=%lu spierr=%lu",
                    (unsigned long)stats.rx_polls,
                    (unsigned long long)(stats.rx_polls ? stats.rx_poll_us/stats.rx_polls : 0),
                    (unsigned long)stats.max_poll_us,(unsigned long)stats.irq_edges,
                    (unsigned long)stats.chip.malformed,(unsigned long)stats.chip.spi_errors);
            }
            if (text && (now>=next_ui || turns!=ui_turns || clicks!=ui_clicks || button_stable!=ui_button) && lvgl_port_lock(2)) {
                /* Do not redraw unchanged text: slow LCD transfers can otherwise
                 * monopolize the shared SPI bus even with no input or CAN traffic. */
                static char buffers[2][256];
                static unsigned current;
                char *buf=buffers[current^1];
                next_ui=now+(test_mcp ? 500000 : 100000);
                ui_turns=turns; ui_clicks=clicks; ui_button=button_stable;
#if CONFIG_BIONX_TEST_BUTTON_GPIO >= 0
                const char *button=button_stable ? "released" : "PRESSED";
#else
                const char *button="disabled (GPIO39 floats)";
#endif
                snprintf(buf,sizeof(buffers[0]),"COMPONENT TEST\nEncoder: %ld\nA/B: %u%u  jumps: %lu\nButton: %s\nClicks: %lu\nMCP 8MHz: %s\nRX: %lu drops: %lu ov: %lu\nCAN PASSIVE / NO TX",
                    (long)turns,(ab>>1)&1,ab&1,(unsigned long)jumps,button,(unsigned long)clicks,state,
                    (unsigned long)stats.chip.received,(unsigned long)stats.capture_dropped,
                    (unsigned long)(stats.chip.overflow0_events+stats.chip.overflow1_events));
                if (!test_mcp)
                    snprintf(buf,sizeof(buffers[0]),"ENCODER TEST\n\nPosition: %ld\nA/B: %u%u\nButton: %s\nClicks: %lu\nInvalid jumps: %lu\nMCP DISABLED",
                        (long)turns,(ab>>1)&1,ab&1,button,(unsigned long)clicks,(unsigned long)jumps);
                if (strcmp(buf,buffers[current])) {
                    lv_label_set_text_static(text,buf);
                    current^=1;
                }
                lvgl_port_unlock();
            }
        }
        vTaskDelay(1);
    }
}
#else
void component_test_run(void) {}
#endif
