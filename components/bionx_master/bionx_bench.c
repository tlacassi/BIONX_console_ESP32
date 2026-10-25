/* SPDX-License-Identifier: GPL-3.0-only */
#include "bionx_bench.h"

static uint8_t trip(bb_control_t *c, bb_reason_t r)
{ c->state=BB_FAULT; c->reason=r; c->output=0; return 0; }
static bb_reason_t check(const bb_control_t *c,const bb_input_t *i,int64_t now)
{
    if (!i || !i->healthy) return BB_NOT_READY;
    if (now<0 || now>INT64_MAX-10000000) return BB_CLOCK;
    if (!bx_sample_fresh(&i->torque,now,150000) || !bx_sample_fresh(&i->speed,now,250000) ||
        !bx_sample_fresh(&i->temperature,now,2000000) || !bx_sample_fresh(&i->voltage_mv,now,2000000) ||
        !bx_sample_fresh(&i->power,now,500000)) return BB_STALE;
    /* Raw speed limit is deliberately not presented as calibrated km/h.
     * Voltage/temperature are candidate scales and require bench verification. */
    if (i->torque.raw>64 || i->speed.raw>=3 || i->temperature.raw>=50 ||
        i->voltage_mv.raw<30000 || i->voltage_mv.raw>42000 || i->power.raw>64) return BB_RANGE;
    if (c->brake_required && (!i->brake_valid || i->brake)) return BB_BRAKE;
    return BB_OK;
}
void bb_init(bb_control_t *c,bool brake_required)
{ if(c) *c=(bb_control_t){.brake_required=brake_required,.torque_us=-1}; }
void bb_disarm(bb_control_t *c)
{
    if (!c) return;
    c->output=0;
    if (c->state!=BB_FAULT) { c->state=BB_IDLE; c->reason=BB_RELEASED; }
}
bool bb_calibrate(bb_control_t *c,const bb_input_t *i,int64_t now)
{
    if (!c || check(c,i,now)!=BB_OK || i->deadman || i->speed.raw || i->torque.raw>8) return false;
    bool brake=c->brake_required;
    bb_init(c,brake); c->state=BB_CALIBRATING; c->last_us=now; c->cal_min=255;
    return true;
}
bool bb_arm(bb_control_t *c,const bb_input_t *i,int64_t now)
{
    if (!c || c->state!=BB_IDLE || !c->calibrated || check(c,i,now)!=BB_OK ||
        i->deadman || i->speed.raw || i->torque.raw>c->baseline+2) return false;
    c->state=BB_READY; c->reason=BB_OK; c->output=0;
    c->last_us=c->ramp_us=now; c->deadline_us=now+5000000;
    c->last_torque=i->torque.raw; c->torque_us=i->torque.timestamp_us;
    return true;
}
uint8_t bb_step(bb_control_t *c,const bb_input_t *i,int64_t now)
{
    if (!c || c->state==BB_FAULT) return 0;
    if (c->state==BB_IDLE) { c->output=0; return 0; }
    if (now<c->last_us || now-c->last_us>100000) return trip(c,BB_CLOCK);
    c->last_us=now;
    bb_reason_t r=check(c,i,now);
    if (r!=BB_OK) return trip(c,r);
    if (i->torque.timestamp_us<c->torque_us) return trip(c,BB_CLOCK);
    if (c->state==BB_CALIBRATING) {
        if (i->deadman || i->speed.raw || i->torque.raw>8) return trip(c,BB_CAL_LOAD);
        if (i->torque.timestamp_us==c->torque_us) return 0;
        c->torque_us=i->torque.timestamp_us;
        if (i->torque.raw<c->cal_min) c->cal_min=i->torque.raw;
        if (i->torque.raw>c->cal_max) c->cal_max=i->torque.raw;
        if (c->cal_max-c->cal_min>2) return trip(c,BB_CAL_LOAD);
        if (++c->cal_count>=16) {
            c->baseline=c->cal_max; c->calibrated=true; c->state=BB_IDLE; c->reason=BB_OK;
        }
        return 0;
    }
    if (i->torque.timestamp_us!=c->torque_us) {
        int delta=(int)i->torque.raw-c->last_torque;
        if (delta>16 || delta < -16) return trip(c,BB_TORQUE_JUMP);
        c->last_torque=i->torque.raw; c->torque_us=i->torque.timestamp_us;
    }
    if (now>=c->deadline_us) {
        c->reason=c->state==BB_READY ? BB_ARM_TIMEOUT : BB_FINISHED;
        c->state=BB_IDLE; c->output=0;
        return 0;
    }
    if (c->state==BB_READY) {
        if (!i->deadman) return 0;
        /* A loaded pedal before pressing the deadman cannot start a burst. */
        if (i->torque.raw>c->baseline+2) return trip(c,BB_CAL_LOAD);
        c->state=BB_RUNNING; c->deadline_us=now+800000; c->ramp_us=now;
    }
    if (!i->deadman) { bb_disarm(c); return 0; }
    unsigned effort=i->torque.raw>c->baseline+2 ? i->torque.raw-c->baseline-2 : 0;
    unsigned target=effort/4;
    if (target>4) target=4;
    /* Never sustain the previous demand on a drop or zero signal. */
    if (target<c->output) c->output=target;
    if (now-c->ramp_us>=100000) {
        if (target>c->output) ++c->output;
        c->ramp_us=now;
    }
    return c->output;
}
bool bb_authorizes(const bb_control_t *c,const bb_input_t *i,uint8_t raw,int64_t now)
{
    return c && c->state==BB_RUNNING && raw>0 && raw<=4 && raw==c->output &&
        now>=c->last_us && now-c->last_us<100000 && now<c->deadline_us &&
        check(c,i,now)==BB_OK && i->deadman;
}
const char *bb_state_name(bb_state_t s)
{
    static const char *n[]={"DISARMED","CALIBRATE","ARMED","HOLD TEST","FAULT"};
    return (unsigned)s<5?n[s]:"UNKNOWN";
}
const char *bb_reason_name(bb_reason_t r)
{
    static const char *n[]={"OK","NOT READY","STALE","RANGE","BRAKE","CLOCK",
        "TORQUE JUMP","CAL LOAD","RELEASED","DONE","ARM TIMEOUT"};
    return (unsigned)r<11?n[r]:"UNKNOWN";
}
