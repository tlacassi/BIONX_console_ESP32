/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include "bionx_jog.h"
static bb_input_t input(int64_t t)
{
    bb_input_t i={.healthy=true,.speed={.valid=true},.temperature={.valid=true,.raw=36},
        .power={.valid=true},.voltage_mv={.valid=true,.raw=39500}};
    i.speed.timestamp_us=i.temperature.timestamp_us=i.power.timestamp_us=i.voltage_mv.timestamp_us=t;
    return i;
}
static int64_t start(bj_control_t *c,bb_input_t *i)
{
    bj_init(c); assert(bj_set_wheel(c,2136)); *i=input(0);
    assert(!bj_step(c,i,0,false,false,0)); /* Boot never starts. */
    assert(!bj_step(c,i,1,false,false,0)); assert(c->target==2);
    for(int64_t t=10000;t<=200000;t+=10000) { *i=input(t); bj_step(c,i,0,false,false,t); }
    assert(c->output==1 && bj_authorizes(c,i,1,200000));
    return 200000;
}
int main(void)
{
    bj_control_t c; bb_input_t i; int64_t t=start(&c,&i);
    for(unsigned level=2;level<=10;++level) {
        i=input(t); bj_step(&c,&i,1,false,false,t);
        assert(c.level==level && c.target==level*2);
    }
    bj_step(&c,&i,1,false,false,t); assert(c.level==10 && c.target==20);
    for(unsigned n=0;n<140;++n) { t+=10000;i=input(t);bj_step(&c,&i,0,false,false,t); }
    assert(c.output==8 && bj_authorizes(&c,&i,8,t));
    i.speed.raw=17; assert(bj_step(&c,&i,0,false,false,t)==4);
    assert(bj_authorizes(&c,&i,4,t) && !bj_authorizes(&c,&i,5,t));
    i.speed.raw=18; assert(!bj_step(&c,&i,0,false,false,t));
    assert(c.state==BJ_RUNNING && c.level==10 && !bj_authorizes(&c,&i,1,t));
    i.speed.raw=0;
    for(unsigned level=9;level>0;--level) {
        bj_step(&c,&i,-1,false,false,t); assert(c.level==level && !c.output);
    }
    assert(!bj_step(&c,&i,-1,false,false,t) && !c.target && !c.level && c.state==BJ_IDLE);
    t=start(&c,&i); i.speed.raw=2;
    for(unsigned n=0;n<300;++n) {
        t+=10000; i=input(t); i.speed.raw=2;
        assert(!bj_step(&c,&i,0,false,false,t));
        assert(c.state==BJ_RUNNING && c.target==2 && !bj_authorizes(&c,&i,1,t));
    }
    for(unsigned n=0;n<20;++n) { t+=10000;i=input(t);bj_step(&c,&i,0,false,false,t); }
    assert(c.state==BJ_RUNNING && c.output==1 && bj_authorizes(&c,&i,1,t));
    i.speed.raw=2; assert(!bj_step(&c,&i,0,false,false,t));
    assert(c.state==BJ_RUNNING && !bj_authorizes(&c,&i,1,t));
    t+=200000; /* A stalled loop still faults, even above target. */
    i=input(t); assert(!bj_step(&c,&i,0,false,false,t) && c.state==BJ_FAULT);
    t=start(&c,&i);
    for(unsigned n=1;n<10;++n) bj_step(&c,&i,1,false,false,t);
    i.speed.raw=21;assert(!bj_step(&c,&i,0,false,false,t) && c.state==BJ_RUNNING);
    i.speed.raw=22;assert(!bj_step(&c,&i,0,false,false,t) && c.reason==BJ_OVERSPEED);
    i.speed.raw=0;bj_step(&c,&i,0,true,false,t);bj_step(&c,&i,0,false,false,t);
    assert(c.state==BJ_IDLE && c.wheel_mm==2136 && !c.output);
    t=start(&c,&i); assert(!bj_step(&c,&i,1,true,false,t));
    assert(!c.output && !c.target && c.state==BJ_IDLE); /* Click takes priority over rotation. */
    t=start(&c,&i);
    assert(c.lease_us==20000000 && c.session_us==20000000);
    while(t<c.lease_us) { t+=10000;i=input(t); bj_step(&c,&i,0,false,false,t); }
    assert(t==20000000 && c.state==BJ_IDLE && c.reason==BJ_TIMEOUT && !c.output);
    t=start(&c,&i);
    while(c.state==BJ_RUNNING) { t+=10000;i=input(t); i.speed.raw=1; bj_step(&c,&i,t%500000==0?1:0,false,false,t); }
    assert(c.reason==BJ_TIMEOUT && t==20000000); /* Turning cannot defeat the 20s session. */
    for(unsigned scenario=0;scenario<10;++scenario) {
        t=start(&c,&i); bool bad=false;
        switch(scenario) {
        case 0:i.healthy=false;break;
        case 1:i.speed.raw=22;break;
        case 2:i.speed.valid=false;break;
        case 3:i.temperature.raw=50;break;
        case 4:i.voltage_mv.raw=29999;break;
        case 5:i.power.raw=17;break;
        case 6:i.brake=true;break;
        case 7:bad=true;break;
        case 8:t+=100001;i=input(t);break;
        case 9:i.speed.timestamp_us=t-150000;break;
        }
        assert(!bj_step(&c,&i,0,false,bad,t) && c.state==BJ_FAULT && !c.target);
        i=input(t); assert(!bj_step(&c,&i,1,false,false,t)); /* Recovery does not restart. */
        assert(!bj_step(&c,&i,0,true,false,t) && c.state==BJ_FAULT); /* Wait for release. */
        i.deadman=true; assert(!bj_reset(&c,&i,t));
        i.deadman=false; assert(bj_reset(&c,&i,t) && c.state==BJ_IDLE && !c.output);
    }
    /* Click acknowledgement is zero-only, stationary, healthy and edge based. */
    t=start(&c,&i); i.healthy=false;
    bj_step(&c,&i,0,false,false,t); assert(c.state==BJ_FAULT);
    i=input(t); i.deadman=true; bj_step(&c,&i,1,true,false,t);
    assert(c.state==BJ_FAULT && c.ack_pending);
    i.deadman=false; bj_step(&c,&i,1,false,false,t);
    assert(c.state==BJ_IDLE && !c.output && !c.target); /* Ignore simultaneous turn. */
    bj_step(&c,&i,1,false,false,t); assert(c.state==BJ_RUNNING);
    i.healthy=false; bj_step(&c,&i,0,false,false,t);
    bj_step(&c,&i,0,true,false,t); bj_step(&c,&i,0,false,false,t);
    assert(c.state==BJ_FAULT && !c.ack_pending);
    i=input(t); bj_step(&c,&i,0,false,false,t); assert(c.state==BJ_FAULT);
    i.speed.raw=1; bj_step(&c,&i,0,true,false,t); bj_step(&c,&i,0,false,false,t);
    assert(c.state==BJ_FAULT); /* Moving wheel cannot be acknowledged. */
    i.speed.raw=0; bj_step(&c,&i,0,true,false,t); bj_step(&c,&i,0,false,true,t);
    assert(c.state==BJ_FAULT); /* Invalid encoder on release. */
    t=start(&c,&i);
    for(unsigned n=1;n<10;++n) bj_step(&c,&i,1,false,false,t);
    while(c.state==BJ_RUNNING) {
        t+=10000;i=input(t);bj_step(&c,&i,0,false,false,t);
    }
    assert(c.reason==BJ_STALL && !c.output && !c.target && t==2000000);
    t=start(&c,&i); assert(!bj_authorizes(&c,&i,1,t+100000));
    assert(!bj_authorizes(&c,&i,9,t));
    i.deadman=true; assert(!bj_authorizes(&c,&i,1,t));
    bj_init(&c); i=input(0);
    assert(!bj_set_wheel(&c,0) && !bj_set_wheel(&c,999) && !bj_set_wheel(&c,4001));
    assert(!bj_step(&c,&i,1,false,false,0) && c.state==BJ_IDLE && !c.target);
    assert(bj_speed_millikmh(&c,0)==UINT32_MAX); /* Unconfigured always inhibited. */
    assert(bj_set_wheel(&c,2136));
    assert(bj_speed_millikmh(&c,0)==0);
    assert(bj_speed_millikmh(&c,1)==1166);
    assert(bj_speed_millikmh(&c,21)==24468);
    assert(bj_speed_millikmh(&c,22)==25633);
    assert(bj_speed_millikmh(&c,256)==UINT32_MAX);
    i.healthy=false;
    assert(!bj_step(&c,&i,1,false,false,0) && !c.target);
    i=input(10000); assert(!bj_step(&c,&i,0,false,false,10000));
    assert(c.reason==BJ_OK && !c.target); /* Ignored turns are not queued. */
    return 0;
}
