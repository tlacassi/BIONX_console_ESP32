/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#ifdef CONFIG_BIONX_ASSIST_V1
#include "bench_app.h"
#include "assist_app.h"
#include "bionx_assist.h"
#include "dashboard.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
static bv_control_t control;
static bb_input_t input;
static int64_t wire_deadline,power_deadline;
static uint8_t wire;
static bool brake_seen;
static int64_t brake_closed_since;
bool bench_app_init(void)
{
    bv_init(&control);
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    gpio_config_t pin={.pin_bit_mask=1ULL<<36,.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    if(gpio_config(&pin)!=ESP_OK) return false;
#endif
    ESP_LOGW("assist","V1 raw gain x2 cap8; auto zero/arm at boot, press stop; wheel-up bench only");
    return true;
}
bool bench_app_command(const char *cmd,int64_t now)
{
    if(!strcmp(cmd,"cal")) {
        ESP_LOGI("assist","Calibration %s",bv_calibrate(&control,&input,now)?"STARTED":"REFUSED");return true;
    }
    if(!strcmp(cmd,"arm")) {
        ESP_LOGI("assist","Arm %s",bv_arm(&control,&input,now)?"OK":"REFUSED");return true;
    }
    if(!strcmp(cmd,"q") || !strcmp(cmd,"stop") || !strcmp(cmd,"disarm")) {
        bv_stop(&control);wire=0;return true;
    }
    return false;
}
void bench_app_tick(bx_master_t *m,const mcp2515_transport_stats_t *s,int64_t now)
{
    input=(bb_input_t){.healthy=m->state==BX_NEUTRAL && !m->stop_requested &&
        !s->fault && s->chip.ready && !s->chip.eflg,.deadman=gpio_get_level(22)==0,
        .brake_valid=true,.torque=m->samples[BX_TORQUE],.speed=m->samples[BX_SPEED],
        .power=m->samples[BX_POWER],.temperature=m->samples[BX_TEMP]};
    float v,w,a;
    if(bx_pack_voltage(m,now,&v)) input.voltage_mv=(bx_sample_t){.valid=true,
        .raw=(m->voltage_pair[0].raw<<8)|m->voltage_pair[1].raw,
        .timestamp_us=m->voltage_pair[0].timestamp_us};
    bool power_ok=bx_battery_power(m,now,&w,&a);
    input.healthy &= power_ok;
    power_deadline=power_ok ? (m->power_voltage_pair[0].timestamp_us<m->current_pair[0].timestamp_us ?
        m->power_voltage_pair[0].timestamp_us:m->current_pair[0].timestamp_us)+500000:0;
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    input.brake=gpio_get_level(36)!=0;
    if(input.brake) { brake_seen=true;brake_closed_since=now; }
    input.brake_valid=brake_seen && !input.brake && now-brake_closed_since>=50000;
#else
    (void)brake_seen;(void)brake_closed_since;
#endif
    if(input.deadman) bv_stop(&control);
    uint8_t wanted=bv_auto_step(&control,&input,now);
#ifdef CONFIG_BIONX_V1_TX
    wire=wanted;
#else
    (void)wanted;wire=0;
#endif
    wire_deadline=now+BV_COMMAND_TTL;
    bx_master_set_assist(m,wire,now);
}
bool bench_app_tx_allowed(const can_frame_t *f,int64_t now)
{
    if(f->data[0] || f->data[2] || f->data[3]>BV_MAX_DRIVE) return false;
    if(!f->data[3]) return true;
#ifdef CONFIG_BIONX_V1_TX
    if(gpio_get_level(22)==0 || now>=wire_deadline || now>=power_deadline || f->data[3]!=wire) return false;
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    if(gpio_get_level(36)!=0) return false;
#endif
    return bv_authorizes(&control,&input,f->data[3],now);
#else
    (void)now;return false;
#endif
}
void bench_app_status(char *text,size_t size)
{
    snprintf(text,size,"V1 %s %s zero%u ped%u want%u cmd%u",bv_state_name(control.state),
        bv_reason_name(control.reason),control.baseline,input.torque.raw,control.target,wire);
}
void assist_app_dashboard(const bx_master_t *m,int64_t now,const char *master_status,uint8_t confirmed_command)
{
    dashboard_live_t d={.status=master_status,.command=confirmed_command,.command_valid=m->state==BX_NEUTRAL,
        .fault=m->state==BX_FAULT || control.state==BV_FAULT};
    if(m->state==BX_NEUTRAL) d.status=control.state==BV_FAULT ? bv_reason_name(control.reason):bv_state_name(control.state);
#ifndef CONFIG_BIONX_V1_TX
    if(m->state==BX_NEUTRAL && control.state==BV_ACTIVE) d.status="DRY RUN TX OFF";
#endif
    d.speed_valid=bx_sample_fresh(&m->samples[BX_SPEED],now,BV_SPEED_TTL);
    d.speed_kmh=bv_speed_millikmh(m->samples[BX_SPEED].raw)*0.001f;
    d.soc_valid=bx_sample_fresh(&m->samples[BX_SOC_VOLTAGE],now,2000000) && m->samples[BX_SOC_VOLTAGE].raw<=100;
    d.battery_soc=m->samples[BX_SOC_VOLTAGE].raw;
    d.motor_level_valid=bx_sample_fresh(&m->samples[BX_POWER],now,500000) && m->samples[BX_POWER].raw<=64;
    d.motor_level=m->samples[BX_POWER].raw;
    d.torque_valid=bx_sample_fresh(&m->samples[BX_TORQUE],now,BV_TORQUE_TTL) && m->samples[BX_TORQUE].raw<=64;
    d.torque=m->samples[BX_TORQUE].raw;
    float a;d.power_valid=bx_battery_power(m,now,&d.battery_power_w,&a);
    dashboard_live_update(&d);
}
#endif
