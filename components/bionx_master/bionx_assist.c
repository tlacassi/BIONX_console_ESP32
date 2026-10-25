/* SPDX-License-Identifier: GPL-3.0-only */
#include "bionx_assist.h"
uint32_t bv_speed_millikmh(uint16_t raw)
{ return ((uint64_t)raw*9091*BV_WHEEL_MM*6+99999)/100000; }
static bv_reason_t check(const bb_input_t *i,int64_t now)
{
    if(now<0 || now>INT64_MAX-10000000) return BV_CLOCK;
    if(!i || !i->healthy || !bx_sample_fresh(&i->torque,now,BV_TORQUE_TTL) ||
       !bx_sample_fresh(&i->speed,now,BV_SPEED_TTL) ||
       !bx_sample_fresh(&i->temperature,now,2000000) ||
       !bx_sample_fresh(&i->voltage_mv,now,2000000) ||
       !bx_sample_fresh(&i->power,now,500000)) return BV_DATA;
    if(i->brake || !i->brake_valid) return BV_BRAKE;
    if(i->torque.raw>64 || i->speed.raw>255 ||
       i->temperature.raw>=50 || i->voltage_mv.raw<30000 ||
       i->voltage_mv.raw>42000 || i->power.raw>64) return BV_RANGE;
    return BV_OK;
}
static uint8_t trip(bv_control_t *c,bv_reason_t r)
{ c->state=BV_FAULT;c->reason=r;c->output=c->target=c->pulse_peak=0;return 0; }
void bv_init(bv_control_t *c)
{ *c=(bv_control_t){.torque_us=-1,.stalled_since=-1,.boot_pending=true}; }
void bv_stop(bv_control_t *c)
{
    c->boot_pending=false;c->output=c->target=c->pulse_peak=0;
    if(c->state!=BV_FAULT) { c->state=BV_IDLE;c->reason=BV_STOPPED; }
}
bool bv_calibrate(bv_control_t *c,const bb_input_t *i,int64_t now)
{
    if(check(i,now)!=BV_OK || i->deadman || i->speed.raw || i->torque.raw>8 || c->state==BV_ACTIVE) return false;
    bv_init(c);c->state=BV_CALIBRATING;c->last_us=now;c->cal_min=255;
    return true;
}
bool bv_arm(bv_control_t *c,const bb_input_t *i,int64_t now)
{
    if(c->state!=BV_READY || !c->calibrated || check(i,now)!=BV_OK || i->deadman ||
       i->speed.raw || i->torque.raw>c->baseline+2) return false;
    c->boot_pending=false;c->state=BV_ACTIVE;c->reason=BV_OK;c->last_us=c->ramp_us=now;
    c->torque_us=i->torque.timestamp_us;c->last_torque=i->torque.raw;c->stalled_since=-1;
    c->output=c->target=c->pulse_peak=0;c->pulse_us=now;return true;
}
static unsigned demand(const bv_control_t *c,const bb_input_t *i)
{
    unsigned effort=i->torque.raw>c->baseline+2 ? i->torque.raw-c->baseline-2 : 0;
    unsigned target=2*effort;
    return target>BV_MAX_DRIVE ? BV_MAX_DRIVE : target;
}
/* A fresh pedal impulse decays by one raw point per 50ms, at most400ms
 * from its peak. Only live, valid telemetry permits this short tail. */
static unsigned pulse_demand(const bv_control_t *c,const bb_input_t *i,int64_t now)
{
    unsigned current=demand(c,i),tail=0;
    if(now>=c->pulse_us && now-c->pulse_us<400000) {
        unsigned fall=(unsigned)((now-c->pulse_us)/50000);
        tail=c->pulse_peak>fall ? c->pulse_peak-fall:0;
    }
    return current>tail ? current:tail;
}
uint8_t bv_step(bv_control_t *c,const bb_input_t *i,int64_t now)
{
    if(c->state==BV_FAULT || c->state==BV_IDLE) return 0;
    if(now<c->last_us || now-c->last_us>BV_COMMAND_TTL) return trip(c,BV_CLOCK);
    c->last_us=now;
    bv_reason_t r=check(i,now);if(r!=BV_OK) return trip(c,r);
    if(i->torque.timestamp_us<c->torque_us) return trip(c,BV_CLOCK);
    if(c->state==BV_CALIBRATING) {
        if(i->deadman || i->speed.raw || i->torque.raw>8) return trip(c,BV_CAL_LOAD);
        if(i->torque.timestamp_us==c->torque_us) return 0;
        c->torque_us=i->torque.timestamp_us;
        if(i->torque.raw<c->cal_min) c->cal_min=i->torque.raw;
        if(i->torque.raw>c->cal_max) c->cal_max=i->torque.raw;
        if(c->cal_max-c->cal_min>2) return trip(c,BV_CAL_LOAD);
        if(++c->cal_count==16) {
            c->baseline=c->cal_max;c->calibrated=true;c->state=BV_READY;
        }
        return 0;
    }
    if(c->state!=BV_ACTIVE) return 0;
    if(i->deadman) { bv_stop(c);return 0; }
    if(i->torque.timestamp_us!=c->torque_us) {
        int delta=(int)i->torque.raw-c->last_torque;
        /* Falling torque always cuts immediately, including a rapid release. */
        if(delta>16) return trip(c,BV_TORQUE_JUMP);
        c->last_torque=i->torque.raw;c->torque_us=i->torque.timestamp_us;
    }
    if(!i->speed.raw && c->output>=4) {
        if(c->stalled_since<0) c->stalled_since=now;
        if(now-c->stalled_since>=2000000) return trip(c,BV_STALL);
    } else c->stalled_since=-1;
    unsigned target=pulse_demand(c,i,now);
    if(demand(c,i)>0 && demand(c,i)>=target) {
        c->pulse_peak=demand(c,i);c->pulse_us=now;
    }
    c->target=target;
    /* Wheel-up V1: no speed cutoff/taper, per explicit bench request. */
    if(c->target<c->output) c->output=c->target;
    if(now-c->ramp_us>=50000) {
        if(c->target>c->output) ++c->output;
        c->ramp_us=now;
    }
    return c->output;
}
/* Only the initial boot is automatic. A stop or a runtime fault cannot rearm
 * itself. Before calibration completes, unstable/loaded pedals restart the
 * observation window with zero command, rather than accepting a loaded zero. */
uint8_t bv_auto_step(bv_control_t *c,const bb_input_t *i,int64_t now)
{
    if(!c->boot_pending || c->state==BV_FAULT) return bv_step(c,i,now);
    if(i && i->deadman) { bv_stop(c);return 0; }
    if(c->state==BV_IDLE) {
        if(bv_calibrate(c,i,now)) return bv_step(c,i,now);
        return 0;
    }
    if(c->state==BV_CALIBRATING) {
        if(check(i,now)!=BV_OK || i->speed.raw || i->torque.raw>8 ||
           (c->cal_count && i->torque.raw+2<c->cal_max) || (c->cal_count && i->torque.raw>c->cal_min+2)) {
            c->state=BV_IDLE;c->cal_count=0;c->torque_us=-1;return 0;
        }
        bv_step(c,i,now);
    }
    if(c->state==BV_READY) bv_arm(c,i,now);
    return 0;
}
bool bv_authorizes(const bv_control_t *c,const bb_input_t *i,uint8_t raw,int64_t now)
{
    if(c->state!=BV_ACTIVE || !raw || raw>BV_MAX_DRIVE || raw!=c->output ||
       now<c->last_us || now-c->last_us>=BV_COMMAND_TTL || check(i,now)!=BV_OK ||
       i->deadman || raw>pulse_demand(c,i,now)) return false;
    if(!i->speed.raw && c->stalled_since>=0 && now-c->stalled_since>=2000000) return false;
    return true;
}
const char *bv_state_name(bv_state_t s)
{
    static const char *n[]={"WAIT ZERO","ZERO PEDALS","WAIT NEUTRAL","ASSIST X2","FAULT"};
    return (unsigned)s<5?n[s]:"UNKNOWN";
}
const char *bv_reason_name(bv_reason_t r)
{
    static const char *n[]={"OK","DATA OLD/LINK","RANGE","LOOP DELAY","PEDALS LOADED",
        "TORQUE JUMP","STOPPED","BRAKE","NO MOTION"};
    return (unsigned)r<9?n[r]:"UNKNOWN";
}
