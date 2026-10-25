/* SPDX-License-Identifier: GPL-3.0-only */
#include "bionx_jog.h"
#define JOG_TIMEOUT_US INT64_C(20000000)
uint32_t bj_speed_millikmh(const bj_control_t *c,uint16_t raw)
{
    if(!c || raw>255 || c->wheel_mm<1000 || c->wheel_mm>4000) return UINT32_MAX;
    /* Candidate motor conversion 9.091rpm/raw; km/h = rpm * mm * 0.00006.
     * Integer millikm/h, rounded upward so the cutoff never rounds down. */
    return (uint32_t)(((uint64_t)raw*c->wheel_mm*54546+99999)/100000);
}
bool bj_set_wheel(bj_control_t *c,uint16_t mm)
{
    if(!c || c->state!=BJ_IDLE || c->output || mm<1000 || mm>4000) return false;
    c->wheel_mm=mm;return true;
}
static bool ready(const bj_control_t *c,const bb_input_t *i,int64_t now)
{
    return i && i->healthy && !i->brake && !i->deadman && now>=0 && now<INT64_MAX-JOG_TIMEOUT_US &&
        bx_sample_fresh(&i->speed,now,150000) && bx_sample_fresh(&i->power,now,500000) &&
        bx_sample_fresh(&i->temperature,now,2000000) && bx_sample_fresh(&i->voltage_mv,now,2000000) &&
        bj_speed_millikmh(c,i->speed.raw)<BJ_OVERSPEED_MILLIKMH && i->power.raw<=16 && i->temperature.raw<50 &&
        i->voltage_mv.raw>=30000 && i->voltage_mv.raw<=42000;
}
static uint8_t fault(bj_control_t *c,bj_reason_t reason)
{ c->state=BJ_FAULT; c->reason=reason; c->output=c->target=c->level=0; return 0; }
void bj_init(bj_control_t *c) { if(c) *c=(bj_control_t){0}; }
void bj_stop(bj_control_t *c)
{
    if(!c) return;
    c->output=c->target=c->level=0;
    if(c->state!=BJ_FAULT) { c->state=BJ_IDLE; c->reason=BJ_CLICK; }
}
bool bj_reset(bj_control_t *c,const bb_input_t *i,int64_t now)
{
    if(!c || !ready(c,i,now) || i->speed.raw || i->deadman) return false;
    uint16_t wheel=c->wheel_mm; bj_init(c); c->wheel_mm=wheel;
    c->last_us=now; return true;
}
uint8_t bj_step(bj_control_t *c,const bb_input_t *i,int delta,bool clicked,bool bad_encoder,int64_t now)
{
    if(!c) return 0;
    if(clicked) {
        if(c->state==BJ_FAULT) c->ack_pending=true;
        bj_stop(c); return 0;
    }
    if(c->state==BJ_FAULT) {
        if(c->ack_pending) {
            c->ack_pending=false;
            if(!bad_encoder && bj_reset(c,i,now)) c->reason=BJ_CLICK;
        }
        return 0; /* Acknowledgement never consumes a start detent. */
    }
    if(c->state==BJ_RUNNING && (now<c->last_us || now-c->last_us>100000)) return fault(c,BJ_CLOCK);
    c->last_us=now;
    if(!ready(c,i,now)) {
        if(c->state==BJ_RUNNING) return fault(c,i && i->speed.valid && bj_speed_millikmh(c,i->speed.raw)>=BJ_OVERSPEED_MILLIKMH?BJ_OVERSPEED:BJ_DATA);
        c->reason=BJ_WAIT; c->output=c->target=c->level=0; return 0;
    }
    if(c->state==BJ_IDLE && c->reason==BJ_WAIT) c->reason=BJ_OK;
    if(bad_encoder || delta < -1 || delta>1) {
        if(c->state==BJ_RUNNING) return fault(c,BJ_ENCODER);
        return 0;
    }
    if(c->state==BJ_RUNNING && (now>=c->lease_us || now>=c->session_us)) {
        c->state=BJ_IDLE; c->reason=BJ_TIMEOUT; c->output=c->target=c->level=0; return 0;
    }
    if(delta) {
        if(c->state==BJ_IDLE) {
            if(delta<0 || i->speed.raw) return 0;
            c->state=BJ_RUNNING; c->session_us=now+JOG_TIMEOUT_US; c->ramp_us=c->motion_us=now;
        }
        if(delta<0) { c->output=0; c->ramp_us=now; }
        int level=c->level+delta;
        c->level=level<0?0:level>BJ_MAX_LEVEL?BJ_MAX_LEVEL:level;
        c->target=c->level*2;
        c->lease_us=now+JOG_TIMEOUT_US; c->reason=BJ_OK;
        if(!c->target) { bj_stop(c); return 0; }
    }
    if(c->state!=BJ_RUNNING) return 0;
    /* Coarse motor RPM converted with configured wheel circumference.
     * Never apply a fixed positive command at target on an unloaded wheel.
     * Limit rolling drive on the first nonzero bin; coast at target, no regen. */
    unsigned cap=c->level<BJ_MAX_DRIVE?c->level:BJ_MAX_DRIVE;
    if(i->speed.raw) {
        c->motion_us=now;
        if(cap>BJ_ROLLING_DRIVE) cap=BJ_ROLLING_DRIVE;
    }
    if(c->output>=4 && now-c->motion_us>=2000000) return fault(c,BJ_STALL);
    if(bj_speed_millikmh(c,i->speed.raw)>=(uint32_t)c->target*1000) { c->output=0; c->ramp_us=now; }
    else {
        if(c->output>cap) c->output=cap;
        if(now-c->ramp_us>=200000) {
            if(c->output<cap) ++c->output;
            c->ramp_us=now;
        }
    }
    return c->output;
}
bool bj_authorizes(const bj_control_t *c,const bb_input_t *i,uint8_t raw,int64_t now)
{
    return c && c->state==BJ_RUNNING && raw>0 && raw<=BJ_MAX_DRIVE && raw<=c->level && raw==c->output &&
        now>=c->last_us && now-c->last_us<100000 && now<c->lease_us && now<c->session_us &&
        ready(c,i,now) && bj_speed_millikmh(c,i->speed.raw)<(uint32_t)c->target*1000 &&
        (!i->speed.raw || raw<=BJ_ROLLING_DRIVE) &&
        (raw<4 || now-c->motion_us<2000000);
}
const char *bj_reason_name(bj_reason_t r)
{
    static const char *n[]={"OK","WAIT DATA","CLICK STOP","TIMEOUT","DATA FAULT","CLOCK","ENCODER","OVERSPEED","NO MOTION"};
    return (unsigned)r<9?n[r]:"UNKNOWN";
}
