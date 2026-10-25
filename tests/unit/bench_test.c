/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include "bionx_bench.h"
static void fresh(bb_input_t *i,int64_t t)
{
    i->torque.timestamp_us=i->speed.timestamp_us=i->temperature.timestamp_us=t;
    i->voltage_mv.timestamp_us=i->power.timestamp_us=t;
}
static bb_input_t input(int64_t t)
{
    bb_input_t i={.healthy=true,.brake_valid=true,.torque={.raw=1,.valid=true},
        .speed={.valid=true},.temperature={.raw=36,.valid=true},
        .voltage_mv={.raw=39500,.valid=true},.power={.valid=true}};
    fresh(&i,t); return i;
}
static int64_t calibrated(bb_control_t *c,bb_input_t *i)
{
    int64_t t=0; *i=input(t); bb_init(c,false);
    assert(!bb_arm(c,i,t));
    assert(bb_calibrate(c,i,t));
    /* Only distinct samples count. */
    assert(!bb_step(c,i,t)); assert(c->cal_count==1);
    assert(!bb_step(c,i,t)); assert(c->cal_count==1);
    for(unsigned n=1;n<16;++n) { t+=50000; fresh(i,t); assert(!bb_step(c,i,t)); }
    assert(c->state==BB_IDLE && c->calibrated && c->baseline==1);
    return t;
}
static int64_t running(bb_control_t *c,bb_input_t *i)
{
    int64_t t=calibrated(c,i);
    assert(bb_arm(c,i,t));
    i->deadman=true; t+=10000; fresh(i,t); assert(!bb_step(c,i,t));
    assert(c->state==BB_RUNNING);
    i->torque.raw=17;
    for (unsigned n=0;n<10;++n) { t+=10000; fresh(i,t); bb_step(c,i,t); }
    assert(c->output==1 && bb_authorizes(c,i,1,t));
    return t;
}
typedef struct { can_frame_t last; unsigned count, torque_reads, speed_reads; bool fail; } wire_t;
static bool wire_send(void *ctx,const can_frame_t *f)
{
    wire_t *w=ctx; w->last=*f; ++w->count;
    if (f->id==0x20 && f->dlc==4 && f->data[1]==9) assert(f->data[3]<=8 && !f->data[2]);
    if(f->dlc==2 && f->id==0x20 && f->data[1]==0x21) ++w->torque_reads;
    if(f->dlc==2 && f->id==0x20 && f->data[1]==0x11) ++w->speed_reads;
    return !w->fail;
}
static void advance(bx_master_t *m,wire_t *w,int64_t *t,unsigned steps)
{
    for(unsigned n=0;n<steps;++n) {
        unsigned before=w->count; bx_master_tick(m,*t);
        if(w->count!=before && w->last.dlc==2) {
            can_frame_t r=w->last; r.id=8; r.dlc=4; r.timestamp_us+=100;
            switch(r.data[1]) {
            case 0x3b: r.data[3]=0x33; break; case 0x3c: r.data[3]=0x69; break;
            case 0x3d: r.data[3]=3; break; case 0x20: r.data[3]=0x60; break;
            case 0xf9:r.data[3]=0x23; break; case 0xfa:r.data[3]=0x28; break;
            case 0xfb:r.data[3]=0xaf; break; case 0xfc:r.data[3]=0xc8; break;
            default:r.data[3]=0;
            }
            bx_master_receive(m,&r);
        }
        *t+=2000;
    }
}
static void master_safety(void)
{
    {
        bx_master_t j; wire_t bus={0}; int64_t time=0;
        bx_config_t jc=bx_default_config(); jc.fast_poll=true; jc.assist_cap=8;
        assert(bx_master_init(&j,&jc,wire_send,&bus));
        assert(bx_master_start(&j,time)); advance(&j,&bus,&time,500);
        assert(j.state==BX_NEUTRAL && bx_master_set_assist(&j,8,time));
        assert(!bx_master_set_assist(&j,9,time));
        advance(&j,&bus,&time,30); assert(j.last_assist==8);
        advance(&j,&bus,&time,80); assert(!j.last_assist);
        jc.assist_cap=9; assert(!bx_master_init(&j,&jc,wire_send,&bus));
    }
    bx_master_t m; wire_t w={0}; int64_t t=0;
    bx_config_t cfg=bx_default_config(); cfg.fast_poll=true; cfg.assist_cap=4;
    bx_config_t bad=cfg; bad.fast_poll=false;
    assert(!bx_master_init(&m,&bad,wire_send,&w));
    bad=cfg; bad.control_us=100000; assert(!bx_master_init(&m,&bad,wire_send,&w));
    assert(bx_master_init(&m,&cfg,wire_send,&w)); assert(bx_master_start(&m,t));
    advance(&m,&w,&t,700); assert(m.state==BX_NEUTRAL);
    assert(w.torque_reads>=12 && w.speed_reads>=6);
    assert(bx_sample_fresh(&m.samples[BX_TORQUE],t,150000));
    assert(bx_sample_fresh(&m.samples[BX_SPEED],t,250000));
    assert(!bx_master_set_assist(&m,5,t));
    assert(!bx_master_set_assist(&m,255,t)); /* Negative signed byte forbidden. */
    assert(bx_master_set_assist(&m,4,t)); advance(&m,&w,&t,30); assert(m.last_assist==4);
    advance(&m,&w,&t,80); assert(m.last_assist==0); /* No refresh: expires to zero. */
    assert(bx_master_set_assist(&m,4,t)); advance(&m,&w,&t,30); assert(m.last_assist==4);
    bx_master_fault(&m,BX_REPLY_TIMEOUT); unsigned before=w.count;
    advance(&m,&w,&t,100);
    assert(m.state==BX_FAULT && w.count==before+2 && !m.last_assist);
    assert(w.last.id==0x20 && w.last.data[1]==9 && !w.last.data[3]);
    assert(!bx_master_set_assist(&m,1,t));
    assert(bx_master_start(&m,t)); advance(&m,&w,&t,500);
    assert(bx_master_set_assist(&m,4,t)); advance(&m,&w,&t,30); assert(m.last_assist==4);
    bx_master_stop(&m); before=w.count; bx_master_tick(&m,t);
    assert(w.count==before+1 && !w.last.data[3] && w.last.data[1]==9);
    /* Unusable transport: no automatic retry/reset pretending to deliver neutral. */
    m.state=BX_NEUTRAL; m.last_assist=4; bx_master_fault(&m,BX_RX_LOSS);
    before=w.count; advance(&m,&w,&t,100); assert(w.count==before);
    m.state=BX_NEUTRAL; m.last_assist=4; bx_master_fault(&m,BX_REPLY_TIMEOUT);
    w.fail=true; before=w.count; advance(&m,&w,&t,100); assert(w.count==before+1);
}
int main(void)
{
    bb_control_t c; bb_input_t i;
    int64_t t=running(&c,&i);
    for (unsigned n=0;n<30;++n) { t+=10000; fresh(&i,t); assert(bb_step(&c,&i,t)<=4); }
    assert(c.output==3); /* effort (17-1-2)/4 = 3, not a constant throttle. */
    i.torque.raw=1; fresh(&i,t); assert(!bb_step(&c,&i,t) && !c.output);
    t=running(&c,&i); i.deadman=false;
    assert(!bb_step(&c,&i,t) && c.state==BB_IDLE && !c.output);
    i.deadman=true; t+=10000; fresh(&i,t); assert(!bb_step(&c,&i,t)); /* No automatic rearm. */
    t=running(&c,&i);
    while (t<c.deadline_us) { t+=10000; fresh(&i,t); bb_step(&c,&i,t); }
    assert(c.state==BB_IDLE && !c.output && c.reason==BB_FINISHED);
    t=calibrated(&c,&i); assert(bb_arm(&c,&i,t));
    while(c.state==BB_READY) { t+=10000; fresh(&i,t); bb_step(&c,&i,t); }
    assert(c.reason==BB_ARM_TIMEOUT);
    for(unsigned scenario=0;scenario<12;++scenario) {
        t=running(&c,&i);
        switch(scenario) {
        case 0: i.healthy=false; break;
        case 1: i.torque.valid=false; break;
        case 2: i.speed.raw=3; break;
        case 3: i.temperature.raw=50; break;
        case 4: i.voltage_mv.raw=29999; break;
        case 5: i.voltage_mv.raw=42001; break;
        case 6: i.torque.raw=65; break;
        case 7: i.torque.raw=40; i.torque.timestamp_us++; t++; break;
        case 8: t+=100001; fresh(&i,t); break;
        case 9: c.brake_required=true; i.brake_valid=false; break;
        case 10: c.brake_required=true; i.brake=true; break;
        case 11: i.torque.timestamp_us=c.torque_us-1; break;
        }
        assert(!bb_step(&c,&i,t) && c.state==BB_FAULT && !c.output);
        i=input(t); t+=1000; fresh(&i,t);
        assert(!bb_step(&c,&i,t) && !bb_arm(&c,&i,t)); /* Fault is latched. */
    }
    /* Final gate rechecks freshness, deadline, brake and hold without ramping. */
    t=running(&c,&i);
    assert(!bb_authorizes(&c,&i,2,t));
    assert(!bb_authorizes(&c,&i,1,t+100000));
    i.deadman=false; assert(!bb_authorizes(&c,&i,1,t));
    t=running(&c,&i); i.torque.timestamp_us=t-150000;
    assert(!bb_authorizes(&c,&i,1,t));
    assert(!bb_step(&c,&i,t) && c.reason==BB_STALE);
    t=calibrated(&c,&i); i.torque.raw=5; assert(!bb_arm(&c,&i,t));
    i=input(0); bb_init(&c,true); assert(bb_calibrate(&c,&i,0));
    i.brake=true; assert(!bb_step(&c,&i,1000) && c.reason==BB_BRAKE);
    i=input(0); bb_init(&c,false); assert(bb_calibrate(&c,&i,0));
    bb_step(&c,&i,0); i.torque.raw=5; fresh(&i,50000);
    assert(!bb_step(&c,&i,50000) && c.reason==BB_CAL_LOAD);
    master_safety();
    return 0;
}
