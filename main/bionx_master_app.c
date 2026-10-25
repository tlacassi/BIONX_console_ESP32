/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#include "bionx_master_app.h"
#ifdef CONFIG_BIONX_CAN_MASTER
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mcp2515_transport.h"
#include "bionx_master.h"
#ifdef CONFIG_BIONX_ENCODER_JOG
#include "bionx_jog.h"
#endif
#include "display.h"
#include "driver/gpio.h"
#include "bionx_button.h"
#include "bench_app.h"
#ifdef CONFIG_BIONX_ASSIST_V1
#include "assist_app.h"
#include "bionx_assist.h"
#include "dashboard.h"
#endif
#include "mcp2515_diagnostic.h"
#include "esp_lvgl_port.h"

static bx_master_t master;
static lv_obj_t *label;
/* Owned by master task: preserve first failed TX even after RX is latched off. */
static bool tx_failure;
static uint8_t confirmed_assist;
static mcp2515_transport_stats_t tx_failure_stats;
static esp_err_t tx_failure_error;
static can_frame_t tx_failure_frame;
static bool transmit(const can_frame_t *f,bool probe)
{
    esp_err_t err=probe ? mcp2515_transport_probe(f,20) : mcp2515_transport_send(f,20);
    if(err!=ESP_OK && !probe && !tx_failure) {
        tx_failure=true; tx_failure_error=err; tx_failure_frame=*f;
        mcp2515_transport_snapshot(&tx_failure_stats);
        const mcp2515_stats_t *s=&tx_failure_stats.chip;
        ESP_LOGE("tx_fault","%s err=%s id=%03lx reg=%02x value=%02x CTRL=%02x INTF=%02x "
            "EFLG=%02x TEC=%u REC=%u mode=%02x",
            mcp2515_tx_failure_name(s,err),esp_err_to_name(err),(unsigned long)f->id,f->data[1],f->data[3],
            s->last_tx_ctrl,s->last_tx_intf,s->eflg,s->tec,s->rec,s->mode);
    }
    if(err==ESP_OK && f->id==0x20 && f->dlc==4 && f->data[1]==9) confirmed_assist=f->data[3];
    return err==ESP_OK;
}
static bool send(void *ctx,const can_frame_t *f)
{
    (void)ctx;
    /* Final gate stays zero-only outside the explicitly verified bench build. */
    if (f->id==0x20 && f->dlc==4 && f->data[1]==0x09 &&
        !bench_app_tx_allowed(f,esp_timer_get_time())) {
        ESP_LOGE("master","Motor command inhibited: sending neutral instead");
        bench_app_command("disarm",esp_timer_get_time());
        can_frame_t neutral=*f;
        neutral.data[0]=neutral.data[2]=neutral.data[3]=0;
        return transmit(&neutral,false);
    }
    return transmit(f,master.state==BX_WAIT_WAKE);
}
static const char *state_name(bx_state_t state)
{
    static const char *names[]={"OFF","STARTING","NEUTRAL","STOPPING","FAULT","WAIT_WAKE"};
    return names[state];
}
static const char *fault_name(bx_fault_t fault)
{
    static const char *names[]={"OK","TX ERROR","NO REPLY","PROFILE",
        "BAD LIMITS","OTHER MASTER","RX/TRANSPORT","CLOCK","BAD REPLY"};
    return (unsigned)fault<sizeof(names)/sizeof(names[0]) ? names[fault] : "UNKNOWN";
}
void bionx_master_app_run(void)
{
    /* RX task priority 5 > master 4 > LVGL 2. UI lock is bounded. */
    vTaskPrioritySet(NULL,4);
#ifdef CONFIG_BIONX_ENCODER_JOG
    ESP_LOGW("master","ENCODER JOG BENCH: low speed, click stop, independent HV disconnect required");
#elif defined(CONFIG_BIONX_ASSIST_BENCH)
    ESP_LOGW("master","PEDAL BENCH CONTROL: inspect DRY RUN / TX permission; external wake button");
#else
    ESP_LOGW("master","NEUTRAL BENCH MASTER; ZERO TORQUE ONLY; external wake button");
#endif
    esp_err_t init_err=display_init();
    if (init_err!=ESP_OK) { ESP_LOGE("master","Display/pins: %s",esp_err_to_name(init_err)); return; }
#ifdef CONFIG_BIONX_ASSIST_V1
    if(dashboard_init()!=ESP_OK) { ESP_LOGE("master","Dashboard init failed");return; }
#else
    if (lvgl_port_lock(1000)) {
        label=lv_label_create(lv_screen_active());
        lv_obj_set_width(label,204);
        lv_obj_set_style_text_font(label,&lv_font_montserrat_12,0);
        lv_obj_set_style_text_line_space(label,2,0);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_center(label);
        lv_label_set_text(label,"NEUTRAL MASTER\nUSB: s=start q=stop");
        lvgl_port_unlock();
    }
#endif
    init_err=mcp2515_transport_start_master();
    if (init_err!=ESP_OK) {
        ESP_LOGE("master","CAN configuration/hardware required: %s",esp_err_to_name(init_err));
#ifdef CONFIG_BIONX_ASSIST_V1
        dashboard_live_t failed={.status="CAN INIT FAIL",.fault=true};dashboard_live_update(&failed);
#endif
        if (label && lvgl_port_lock(1000)) {
            lv_label_set_text(label,"CAN CONFIG REQUIRED\nVerify MCP module/quartz");
            lvgl_port_unlock();
        }
        return;
    }
    bx_config_t cfg=bx_default_config();
#ifdef CONFIG_BIONX_ASSIST_BENCH
    cfg.fast_poll=true;
    cfg.assist_cap=4;
#ifdef CONFIG_BIONX_ENCODER_JOG
    cfg.assist_cap=BJ_MAX_DRIVE; /* Shared jog cap: startup8, rolling4, zero at target. */
    cfg.measure_power=true;
#endif
#endif
#ifdef CONFIG_BIONX_ASSIST_V1
    cfg.assist_cap=BV_MAX_DRIVE;cfg.measure_power=true;cfg.pedal_control=true;
#endif
    cfg.reply_us=CONFIG_BIONX_MASTER_REPLY_MS*1000;
    cfg.write_gap_us=CONFIG_BIONX_MASTER_WRITE_GAP_MS*1000;
    cfg.control_us=CONFIG_BIONX_MASTER_CONTROL_MS*1000;
    cfg.battery_us=CONFIG_BIONX_MASTER_BATTERY_MS*1000;
    if (!bx_master_init(&master,&cfg,send,NULL)) {
        ESP_LOGE("master","Invalid timing configuration"); return;
    }
#ifdef CONFIG_BIONX_MASTER_ENCODER_BUTTON
    const gpio_config_t button_pin={.pin_bit_mask=1ULL<<22,.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,
        .intr_type=GPIO_INTR_DISABLE};
    if (gpio_config(&button_pin)!=ESP_OK) {
        ESP_LOGE("master","Button initialization failed; master stays OFF"); return;
    }
    bx_button_t button={0};
#endif
    if (!bench_app_init()) { ESP_LOGE("master","Bench input initialization failed"); return; }
    const int flags=fcntl(STDIN_FILENO,F_GETFL,0);
    if (flags<0 || fcntl(STDIN_FILENO,F_SETFL,flags|O_NONBLOCK)<0) {
#ifndef CONFIG_BIONX_MASTER_ENCODER_BUTTON
        ESP_LOGE("master","No nonblocking USB/serial input; master stays OFF"); return;
#else
        ESP_LOGW("master","Serial input unavailable; encoder control remains available");
#endif
    }
    const bool serial_ready=flags>=0 && (fcntl(STDIN_FILENO,F_GETFL,0)&O_NONBLOCK);
    int64_t quiet_until=esp_timer_get_time()+1000000, next_ui=0;
    bool transport_failed=false, command_overflow=false;
    const char *start_hint="Short click: wait wake";
    bool auto_discover=true;
    char command[16]={0}; unsigned command_len=0;
    ESP_LOGI("master","Auto WAIT_WAKE: battery query every 1s; external wake starts neutral init. Short/s=rearm, hold 2s/q=stop.");
    for (;;) {
        can_frame_t frame;
        for (unsigned budget=0;budget<64 && mcp2515_capture_receive(&frame,0);++budget) {
            quiet_until=esp_timer_get_time()+1000000;
            bx_master_receive(&master,&frame);
        }
        mcp2515_transport_stats_t stats={0};
        if (mcp2515_transport_snapshot(&stats)!=ESP_OK || stats.fault ||
            stats.chip.spi_errors || stats.chip.mode_faults || stats.chip.bus_off ||
            !stats.chip.ready || stats.capture_dropped || stats.chip.overflow0_events || stats.chip.overflow1_events) {
            transport_failed=true;
            bx_master_fault(&master,BX_RX_LOSS);
        }
        const int64_t now=esp_timer_get_time();
#ifdef CONFIG_BIONX_MASTER_ENCODER_BUTTON
        bx_button_event_t event=bx_button_tick(&button,gpio_get_level(22)==0,now);
        if (event==BX_BUTTON_STOP) { auto_discover=false; bx_master_stop(&master); }
        if (event==BX_BUTTON_START && (master.state==BX_OFF || master.state==BX_FAULT)) {
            if (now>=quiet_until && !transport_failed) {
                bx_master_discover(&master,now); auto_discover=false;
            }
            else {
                start_hint=transport_failed ? "Transport: reset needed" : "Bus busy: click again";
                ESP_LOGW("master","Button start refused: traffic present or transport fault");
            }
        }
#endif
        unsigned char ch;
        for (unsigned budget=0;budget<32 && serial_ready && read(STDIN_FILENO,&ch,1)==1;++budget) {
            if (ch=='\n' || ch=='\r') {
                command[command_len]=0;
                if (!command_overflow) bench_app_command(command,now);
                if (!command_overflow && (!strcmp(command,"q") || !strcmp(command,"stop"))) {
                    auto_discover=false; bx_master_stop(&master);
                }
                if (!command_overflow && (!strcmp(command,"s") || !strcmp(command,"start"))) {
                    if (now>=quiet_until && !transport_failed) {
                        bx_master_discover(&master,now); auto_discover=false;
                    }
                    else ESP_LOGW("master","Start refused: traffic present or transport fault");
                }
                command_len=0; command_overflow=false;
            } else if (command_len<sizeof(command)-1) command[command_len++]=ch;
            else command_overflow=true;
        }
        if (auto_discover && master.state==BX_OFF && now>=quiet_until && !transport_failed) {
            auto_discover=false;
            bx_master_discover(&master,now);
        }
        bench_app_tick(&master,&stats,now);
        bx_master_tick(&master,now);
        if (now>=next_ui) {
            next_ui=now+250000;
            char temp[24]="--", soc[24]="--", voltage[24]="--", progress[48];
            unsigned fresh=0;
            for (unsigned i=0;i<8;++i)
                if (bx_sample_fresh(&master.samples[i],now,cfg.sample_ttl_us)) ++fresh;
            if (bx_sample_fresh(&master.samples[BX_TEMP],now,cfg.sample_ttl_us))
                snprintf(temp,sizeof(temp),"%u C?",master.samples[BX_TEMP].raw);
            if (bx_sample_fresh(&master.samples[BX_SOC_VOLTAGE],now,cfg.sample_ttl_us))
                snprintf(soc,sizeof(soc),"%u %%?",master.samples[BX_SOC_VOLTAGE].raw);
            float v;
            if (bx_pack_voltage(&master,now,&v)) snprintf(voltage,sizeof(voltage),"%.2f V?",(double)v);
            if (master.state==BX_WAIT_WAKE) snprintf(progress,sizeof(progress),"Press external wake");
            else if (master.waiting)
                snprintf(progress,sizeof(progress),"Step %u wait %02X/%02X",master.step,
                    master.expected_node,master.expected_reg);
            else if (master.state==BX_OFF) snprintf(progress,sizeof(progress),"%s",start_hint);
            else snprintf(progress,sizeof(progress),"Step %u  fresh %u/8",master.step,fresh);
            const unsigned long losses=(unsigned long)stats.capture_dropped+
                stats.chip.overflow0_events+stats.chip.overflow1_events;
            const long rx_age=stats.last_rx_us>0 ? (long)((now-stats.last_rx_us)/1000) : -1;
            ESP_LOGI("master","%s fault=%s step=%u wait=%d %02x/%02x TX=%lu RX=%lu age=%ldms "
                "pack=%s temp=%s SOC=%s fresh=%u/8 loss=%lu EFLG=%02x TEC=%u REC=%u txerr=%lu timeout=%lu maxpoll=%luus TXCTRL=%02x TXINTF=%02x",
                state_name(master.state),fault_name(master.fault),master.step,master.waiting,
                master.expected_node,master.expected_reg,(unsigned long)stats.chip.transmitted,
                (unsigned long)stats.chip.received,rx_age,voltage,temp,soc,fresh,losses,
                stats.chip.eflg,stats.chip.tec,stats.chip.rec,
                (unsigned long)stats.chip.tx_errors,(unsigned long)stats.chip.tx_timeouts,
                (unsigned long)stats.max_poll_us,stats.chip.last_tx_ctrl,stats.chip.last_tx_intf);
            char bench_text[224];
            bench_app_status(bench_text,sizeof(bench_text));
#ifdef CONFIG_BIONX_ASSIST_BENCH
            ESP_LOGI("bench","%s",bench_text);
#endif
#ifdef CONFIG_BIONX_ASSIST_V1
            assist_app_dashboard(&master,now,tx_failure ? "TX FAULT" :
                master.state==BX_FAULT ? fault_name(master.fault):state_name(master.state),confirmed_assist);
#endif
            /* LVGL copies this task-local buffer while locked. */
            if (label && lvgl_port_lock(5)) {
                char ui_text[512];
                snprintf(ui_text,sizeof(ui_text),
                    "%s\n%s / %s\nBatt: %s\nMotor: %s  SOC: %s\n"
                    "TX %lu RX %lu\nRX %ld ms fresh %u/8\nLoss %lu TEC%u REC%u\n%s",
                    bench_text,state_name(master.state),fault_name(master.fault),voltage,temp,soc,
                    (unsigned long)stats.chip.transmitted,(unsigned long)stats.chip.received,
                    rx_age,fresh,losses,stats.chip.tec,stats.chip.rec,progress);
                if(tx_failure) {
                    const mcp2515_stats_t *s=&tx_failure_stats.chip;
                    snprintf(ui_text,sizeof(ui_text),
                        "CONTROL OFF / TX FAULT\n%s\nERR %s\nID %03lX REG %02X VAL %02X\n"
                        "CTRL %02X INTF %02X\nEFLG %02X MODE %02X\nTEC %u REC %u\nTX %lu RX %lu\n"
                        "First error frozen\nNo automatic rearm",
                        mcp2515_tx_failure_name(s,tx_failure_error),esp_err_to_name(tx_failure_error),
                        (unsigned long)tx_failure_frame.id,tx_failure_frame.data[1],tx_failure_frame.data[3],
                        s->last_tx_ctrl,s->last_tx_intf,s->eflg,s->mode,s->tec,s->rec,
                        (unsigned long)s->transmitted,(unsigned long)s->received);
                }
                lv_label_set_text(label,ui_text);
                lvgl_port_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5)>0 ? pdMS_TO_TICKS(5) : 1);
    }
}
#else
void bionx_master_app_run(void) {}
#endif
