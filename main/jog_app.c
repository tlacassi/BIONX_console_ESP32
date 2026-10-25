/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#ifdef CONFIG_BIONX_ENCODER_JOG
#include "bench_app.h"
#include "bionx_jog.h"
#include "esp_timer.h"
#include "bionx_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
static bj_control_t jog;
static bb_input_t input;
static char battery_power[48]="Pbat -- W";
static unsigned speed_peak,torque_peak;
static int64_t power_valid_until;
static char fault_detail[48];
static bx_encoder_t encoder;
static portMUX_TYPE encoder_lock=portMUX_INITIALIZER_UNLOCKED;
static void encoder_interrupt(void *arg)
{
    (void)arg;
    portENTER_CRITICAL_ISR(&encoder_lock);
    bx_encoder_button(&encoder,gpio_get_level(22)==0);
    bx_encoder_edge(&encoder,(gpio_get_level(25)<<1)|gpio_get_level(26));
    portEXIT_CRITICAL_ISR(&encoder_lock);
}
static uint8_t wire_command;
static int64_t wire_deadline;
#ifdef CONFIG_BIONX_ASSIST_BRAKE
static bool brake_open_seen,brake_closed_stable;
static int64_t brake_release_since;
#endif
bool bench_app_init(void)
{
    const gpio_config_t p={.pin_bit_mask=(1ULL<<25)|(1ULL<<26),.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    if(gpio_config(&p)!=ESP_OK) return false;
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    gpio_config_t b={.pin_bit_mask=1ULL<<36,.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    if(gpio_config(&b)!=ESP_OK) return false;
#endif
    bx_encoder_init(&encoder,(gpio_get_level(25)<<1)|gpio_get_level(26),gpio_get_level(22)==0);
    esp_err_t err=gpio_install_isr_service(0);
    if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) return false;
    const gpio_num_t pins[]={GPIO_NUM_25,GPIO_NUM_26,GPIO_NUM_22};
    unsigned added=0;
    for(;added<3;++added) {
        err=gpio_isr_handler_add(pins[added],encoder_interrupt,NULL);
        if(err!=ESP_OK) break;
    }
    if(err==ESP_OK) {
        for(unsigned n=0;n<3;++n) {
            err=gpio_set_intr_type(pins[n],GPIO_INTR_ANYEDGE);
            if(err==ESP_OK) err=gpio_intr_enable(pins[n]);
            if(err!=ESP_OK) break;
        }
    }
    if(err!=ESP_OK) {
        for(unsigned n=0;n<added;++n) { gpio_intr_disable(pins[n]); gpio_isr_handler_remove(pins[n]); }
        return false;
    }
    bj_init(&jog);
    if(!bj_set_wheel(&jog,CONFIG_BIONX_JOG_WHEEL_MM))
        ESP_LOGE("jog","Wheel circumference required: drive inhibited");
    ESP_LOGW("jog","WHEEL-UP BENCH: target0..20km/h estimated, overspeed25km/h, command0..8 (rolling max4), click neutral, idle20s/session20s stop; CAN-loss stop unverified");
    return true;
}
void bench_app_tick(bx_master_t *m,const mcp2515_transport_stats_t *s,int64_t now)
{
    input=(bb_input_t){.healthy=m->state==BX_NEUTRAL && !m->stop_requested && !s->fault &&
        s->chip.ready && !s->chip.eflg,.deadman=gpio_get_level(22)==0,
        .torque=m->samples[BX_TORQUE],.speed=m->samples[BX_SPEED],.power=m->samples[BX_POWER],.temperature=m->samples[BX_TEMP]};
    if(bx_sample_fresh(&input.torque,now,1000000) && input.torque.raw>torque_peak)
        torque_peak=input.torque.raw;
    float v;
    if(bx_pack_voltage(m,now,&v)) input.voltage_mv=(bx_sample_t){
        .raw=(m->voltage_pair[0].raw<<8)|m->voltage_pair[1].raw,
        .timestamp_us=m->voltage_pair[0].timestamp_us,.valid=true};
    float watts,amps;
    bool valid_power=bx_battery_power(m,now,&watts,&amps);
    input.healthy=input.healthy && valid_power;
    power_valid_until=valid_power ?
        (m->power_voltage_pair[0].timestamp_us<m->current_pair[0].timestamp_us ?
            m->power_voltage_pair[0].timestamp_us:m->current_pair[0].timestamp_us)+500000 : 0;
    if(valid_power)
        snprintf(battery_power,sizeof(battery_power),"Pbat %.1f W? I%+.2f A",(double)watts,(double)amps);
    else snprintf(battery_power,sizeof(battery_power),"Pbat -- W (no fresh U/I)");
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    input.brake=gpio_get_level(36)!=0;
    if(input.brake) { brake_open_seen=true; brake_closed_stable=false; brake_release_since=now; }
    else if(now-brake_release_since>=50000) brake_closed_stable=true;
    input.brake_valid=brake_open_seen && brake_closed_stable;
    input.healthy=input.healthy && input.brake_valid;
#endif
    portENTER_CRITICAL(&encoder_lock);
    bx_encoder_batch_t batch=bx_encoder_take(&encoder);
    portEXIT_CRITICAL(&encoder_lock);
    const bool clicked=input.deadman || batch.stop;
    const bool was_running=jog.state==BJ_RUNNING;
    const bool was_fault=jog.state==BJ_FAULT;
    uint8_t wanted=bj_step(&jog,&input,0,clicked,batch.bad,now);
    if(was_running && jog.state==BJ_FAULT && jog.reason==BJ_DATA) {
        const char *cause="RANGE"; int64_t age=-1;
        if(m->state!=BX_NEUTRAL || s->fault || !s->chip.ready || s->chip.eflg) cause="LINK/MASTER";
        else if(!bx_sample_fresh(&input.speed,now,150000)) { cause="SPEED OLD"; age=now-input.speed.timestamp_us; }
        else if(!bx_sample_fresh(&input.power,now,500000)) { cause="P14 OLD"; age=now-input.power.timestamp_us; }
        else if(!bx_sample_fresh(&input.temperature,now,2000000)) { cause="TEMP OLD"; age=now-input.temperature.timestamp_us; }
        else if(!bx_sample_fresh(&input.voltage_mv,now,2000000)) { cause="VOLT OLD"; age=now-input.voltage_mv.timestamp_us; }
        else if(!valid_power) { cause="U/I INVALID"; age=now-m->power_voltage_pair[0].timestamp_us; }
        else if(input.power.raw>16) cause="P14 RANGE";
        else if(input.temperature.raw>=50) cause="TEMP RANGE";
        else if(input.voltage_mv.raw<30000 || input.voltage_mv.raw>42000) cause="VOLT RANGE";
        snprintf(fault_detail,sizeof(fault_detail),"DATA %s %ldms",cause,(long)(age/1000));
        ESP_LOGE("jog","%s frozen: speed=%u P14=%u voltage=%u U/I=%d",fault_detail,
            input.speed.raw,input.power.raw,input.voltage_mv.raw,valid_power);
    }
    const bool accept_steps=!was_fault && (!was_running || jog.state==BJ_RUNNING);
    if(!clicked && !batch.bad && accept_steps) {
        /* All queued detents are genuine complete Gray-code cycles. Process
         * each once; multiple detents are not an invalid delta>1. */
        for(unsigned n=0;n<batch.count;++n)
            wanted=bj_step(&jog,&input,batch.steps[n],false,false,now);
    }
#ifdef CONFIG_BIONX_JOG_TX
    wire_command=wanted;
#else
    (void)wanted; wire_command=0;
#endif
    if(!was_running && jog.state==BJ_RUNNING) speed_peak=0;
    if((was_running || jog.state==BJ_RUNNING) && input.speed.valid && input.speed.raw>speed_peak)
        speed_peak=input.speed.raw;
    wire_deadline=now+100000;
    bx_master_set_assist(m,wire_command,now);
}
bool bench_app_command(const char *cmd,int64_t now)
{
    if(!strcmp(cmd,"reset")) {
        input.deadman=gpio_get_level(22)==0;
        bool ok=bj_reset(&jog,&input,now);
        ESP_LOGW("jog","Reset %s: must be stationary with valid data and button released",ok?"OK":"REFUSED");
        return true;
    }
    if(!strcmp(cmd,"disarm") || !strcmp(cmd,"q") || !strcmp(cmd,"stop")) {
        bj_stop(&jog); wire_command=0; return true;
    }
    return false;
}
bool bench_app_tx_allowed(const can_frame_t *f,int64_t now)
{
    if(f->data[0] || f->data[2] || f->data[3]>BJ_MAX_DRIVE) return false;
    if(!f->data[3]) return true;
#ifdef CONFIG_BIONX_JOG_TX
    portENTER_CRITICAL(&encoder_lock);
    bool bad_encoder=encoder.bad;
    bool stop_requested=encoder.stop || encoder.pressed;
    portEXIT_CRITICAL(&encoder_lock);
    if(bad_encoder) {
        /* A last-moment invalid edge must latch the fault before the generic
         * neutral substitution disarms the controller. */
        bj_step(&jog,&input,0,false,true,now);
        return false;
    }
    if(stop_requested || gpio_get_level(22)==0 || now>=power_valid_until || now>=wire_deadline || f->data[3]!=wire_command) return false;
#ifdef CONFIG_BIONX_ASSIST_BRAKE
    if(!input.brake_valid || gpio_get_level(36)!=0) return false;
#endif
    return bj_authorizes(&jog,&input,f->data[3],now);
#else
    (void)now; return false;
#endif
}
void bench_app_status(char *text,size_t size)
{
#ifdef CONFIG_BIONX_JOG_TX
    const char *mode="JOG LIVE";
#else
    const char *mode="JOG DRY RUN";
#endif
    portENTER_CRITICAL(&encoder_lock);
    unsigned long invalid=encoder.invalid,overflow=encoder.overflow;
    portEXIT_CRITICAL(&encoder_lock);
    char speed[16]="--",peak[16]="--",pedal[48];
    int64_t now=esp_timer_get_time();
    if(bx_sample_fresh(&input.torque,now,1000000))
        snprintf(pedal,sizeof(pedal),"Ped raw%u max%u %ldms",input.torque.raw,torque_peak,
            (long)((now-input.torque.timestamp_us)/1000));
    else snprintf(pedal,sizeof(pedal),"Ped -- stale max%u",torque_peak);
    if(jog.wheel_mm && bx_sample_fresh(&input.speed,esp_timer_get_time(),150000))
        snprintf(speed,sizeof(speed),"%.1f",bj_speed_millikmh(&jog,input.speed.raw)*0.001);
    if(jog.wheel_mm) snprintf(peak,sizeof(peak),"%.1f",bj_speed_millikmh(&jog,speed_peak)*0.001);
    snprintf(text,size,"%s %s\nL%u/10 T%u V%s~ km/h\n%s (CLICK STOP)\n%s\n%s\ncmd%u max%s~ E%lu O%lu",mode,
        jog.state==BJ_FAULT?"FAULT":jog.state==BJ_RUNNING?"ACTIVE":"READY",jog.level,jog.target,
        speed,
        jog.state==BJ_FAULT && jog.reason==BJ_DATA ? fault_detail:bj_reason_name(jog.reason),pedal,battery_power,wire_command,peak,invalid,overflow);
}
#endif
