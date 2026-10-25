/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#include "bench_app.h"
#include <stdio.h>
#include <string.h>
#if defined(CONFIG_BIONX_ASSIST_BENCH) && !defined(CONFIG_BIONX_ENCODER_JOG) && !defined(CONFIG_BIONX_ASSIST_V1)
#include "bionx_bench.h"
#include "driver/gpio.h"
#include "esp_log.h"
static bb_control_t control;
static bb_input_t input;
static bool initialized;
static uint8_t wire_command;
static int64_t wire_deadline;
#ifdef CONFIG_BIONX_ASSIST_BRAKE
static bool brake_applied,brake_released;
static int64_t release_since;
#endif
bool bench_app_init(void)
{
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    const gpio_config_t p={.pin_bit_mask=1ULL<<36,.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    if (gpio_config(&p)!=ESP_OK) return false;
    bb_init(&control,true);
#else
    bb_init(&control,false);
#endif
    initialized=true;
    ESP_LOGW("bench","BENCH ONLY: cal unloaded, arm then hold encoder; 800ms max, raw cap4, no regen");
#ifndef CONFIG_BIONX_ASSIST_TX_VERIFIED
    ESP_LOGW("bench","DRY RUN: hardware timeout/power disconnect not verified; actual CAN assistance locked to zero");
#endif
    return true;
}
void bench_app_tick(bx_master_t *m,const mcp2515_transport_stats_t *stats,int64_t now)
{
    if (!initialized) return;
    input=(bb_input_t){.healthy=m->state==BX_NEUTRAL && !m->stop_requested &&
        !stats->fault && stats->chip.ready && !stats->chip.eflg,
        .deadman=gpio_get_level(22)==0,.torque=m->samples[BX_TORQUE],
        .speed=m->samples[BX_SPEED],.temperature=m->samples[BX_TEMP],.power=m->samples[BX_POWER]};
    float volts;
    if (bx_pack_voltage(m,now,&volts)) {
        input.voltage_mv=(bx_sample_t){.raw=(m->voltage_pair[0].raw<<8)|m->voltage_pair[1].raw,
            .timestamp_us=m->voltage_pair[0].timestamp_us,.valid=true};
    }
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    input.brake=gpio_get_level(36)!=0;
    if (input.brake) { brake_applied=true; brake_released=false; release_since=now; }
    else if (now-release_since>=50000) brake_released=true;
    input.brake_valid=brake_applied && brake_released;
#endif
    uint8_t proposed=bb_step(&control,&input,now);
#ifdef CONFIG_BIONX_ASSIST_TX_VERIFIED
    wire_command=proposed;
#else
    (void)proposed;
    wire_command=0;
#endif
    wire_deadline=now+100000;
    bx_master_set_assist(m,wire_command,now);
}
bool bench_app_command(const char *cmd,int64_t now)
{
    if (!initialized) return false;
    input.deadman=gpio_get_level(22)==0;
    if (!strcmp(cmd,"cal")) {
        bool ok=bb_calibrate(&control,&input,now);
        wire_command=0;
        ESP_LOGW("bench","Calibration %s: wheel stationary, pedals fully unloaded",ok?"started":"REFUSED");
        return true;
    }
    if (!strcmp(cmd,"arm")) {
        bool ok=bb_arm(&control,&input,now);
        ESP_LOGW("bench","Arm %s (%s): hold encoder only after neutral calibration",
            ok?"accepted":"REFUSED",bb_reason_name(control.reason));
        return true;
    }
    if (!strcmp(cmd,"disarm") || !strcmp(cmd,"q") || !strcmp(cmd,"stop")) {
        bb_disarm(&control); wire_command=0;
        return true;
    }
    return false;
}
bool bench_app_tx_allowed(const can_frame_t *f,int64_t now)
{
    if (f->data[0] || f->data[2] || f->data[3]>4) return false;
    if (!f->data[3]) return true;
#ifdef CONFIG_BIONX_ASSIST_TX_VERIFIED
    if (!initialized || control.state!=BB_RUNNING || f->data[3]!=wire_command ||
        now<control.last_us || now>=wire_deadline || now>=control.deadline_us ||
        gpio_get_level(22)!=0) return false;
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    if (!input.brake_valid || gpio_get_level(36)!=0) return false;
#endif
    /* Re-evaluate freshness/limits at the last point before the SPI send. */
    return bb_authorizes(&control,&input,f->data[3],now);
#else
    (void)now; return false;
#endif
}
void bench_app_status(char *text,size_t size)
{
#ifdef CONFIG_BIONX_ASSIST_TX_VERIFIED
    const char *mode="TX BENCH";
#else
    const char *mode="DRY RUN TX=0";
#endif
    snprintf(text,size,"%s %s\nTq%u base%u want%u tx%u\n%s",mode,bb_state_name(control.state),
        input.torque.raw,control.baseline,control.output,wire_command,bb_reason_name(control.reason));
}
#elif !defined(CONFIG_BIONX_ENCODER_JOG) && !defined(CONFIG_BIONX_ASSIST_V1)
bool bench_app_init(void) { return true; }
void bench_app_tick(bx_master_t *m,const mcp2515_transport_stats_t *s,int64_t now) { (void)m;(void)s;(void)now; }
bool bench_app_command(const char *c,int64_t now) { (void)c;(void)now;return false; }
bool bench_app_tx_allowed(const can_frame_t *f,int64_t now) { (void)now;return !f->data[0] && !f->data[2] && !f->data[3]; }
void bench_app_status(char *text,size_t size) { snprintf(text,size,"ZERO TORQUE"); }
#endif
