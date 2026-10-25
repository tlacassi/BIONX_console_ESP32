/* SPDX-License-Identifier: GPL-3.0-only */
#include <math.h>
#include "bionx_control.h"
static bool valid(const bx_control_t *c,const bx_control_input_t *i,int64_t now)
{
    return i && i->profile_validated && i->brake_valid && i->measurements_valid &&
        i->timestamp_us>=0 && now>=i->timestamp_us && now-i->timestamp_us<c->config.ttl_us &&
        isfinite(i->speed_kmh) && i->speed_kmh>=0 &&
        isfinite(i->temperature_c) && i->temperature_c<c->config.max_temperature &&
        isfinite(i->voltage) && i->voltage>=c->config.min_voltage && i->voltage<=c->config.max_voltage &&
        isfinite(i->current_a) && i->current_a<=c->config.max_discharge_a &&
        i->current_a>=-c->config.max_charge_a && i->rider_effort<=64 &&
        i->requested>=-64 && i->requested<=64;
}
bool bx_control_init(bx_control_t *c,const bx_control_config_t *cfg)
{
    if (!c || !cfg || cfg->max_assist<0 || cfg->max_assist>64 || cfg->max_regen<0 || cfg->max_regen>64 ||
        cfg->ramp_step<1 || cfg->ramp_step>64 || cfg->ttl_us<1000 || cfg->period_us<1000 ||
        cfg->ttl_us<cfg->period_us || !isfinite(cfg->max_speed) || cfg->max_speed<=0 ||
        !isfinite(cfg->max_temperature) || !isfinite(cfg->min_voltage) || !isfinite(cfg->max_voltage) ||
        cfg->min_voltage<=0 || cfg->max_voltage<=cfg->min_voltage ||
        !isfinite(cfg->max_discharge_a) || cfg->max_discharge_a<=0 ||
        !isfinite(cfg->max_charge_a) || cfg->max_charge_a<=0) return false;
    *c=(bx_control_t){.config=*cfg}; return true;
}
bool bx_control_arm(bx_control_t *c,const bx_control_input_t *i,int64_t now)
{
    if (!c || now<0 || !valid(c,i,now) || i->requested || i->brake) return false;
    c->state=BX_CONTROL_ARMED; c->output=0; c->last_us=now; return true;
}
int8_t bx_control_step(bx_control_t *c,const bx_control_input_t *i,int64_t now)
{
    if (!c || c->state!=BX_CONTROL_ARMED) return 0;
    if (now<c->last_us || now-c->last_us>c->config.ttl_us || !valid(c,i,now)) {
        c->state=BX_CONTROL_FAULT; c->output=0; return 0;
    }
    int target=i->requested;
    if (target>0) {
        if (i->brake || !i->battery_discharge_allowed || i->speed_kmh>=c->config.max_speed) target=0;
        if (target>i->rider_effort) target=i->rider_effort;
        if (target>c->config.max_assist) target=c->config.max_assist;
    } else if (target<0) {
        if (!i->battery_charge_allowed || i->voltage>=c->config.max_voltage || i->speed_kmh==0) target=0;
        if (target < -c->config.max_regen) target=-c->config.max_regen;
    }
    /* Returning toward zero is immediate, including a brake at the same timestamp. */
    if ((c->output>0 && target<=0) || (c->output<0 && target>=0)) c->output=0;
    if (c->output>0 && target<c->output) c->output=target;
    if (c->output<0 && target>c->output) c->output=target;
    if (now-c->last_us>=c->config.period_us) {
        if (target>c->output) {
            c->output+=c->config.ramp_step;
            if (c->output>target) c->output=target;
        } else if (target<c->output) {
            c->output-=c->config.ramp_step;
            if (c->output<target) c->output=target;
        }
        c->last_us=now;
    }
    return (int8_t)c->output;
}
