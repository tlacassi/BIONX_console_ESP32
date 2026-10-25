/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include "bionx_assist.h"
static bb_input_t input(int64_t t,unsigned tq,unsigned speed)
{
    bb_input_t i={.healthy=true,.brake_valid=true};
    i.torque=(bx_sample_t){tq,t,true};i.speed=(bx_sample_t){speed,t,true};
    i.temperature=(bx_sample_t){35,t,true};i.voltage_mv=(bx_sample_t){39500,t,true};
    i.power=(bx_sample_t){0,t,true};return i;
}
static int64_t ready(bv_control_t *c)
{
    bv_init(c);bb_input_t i=input(0,1,0);assert(!bv_arm(c,&i,0));
    assert(bv_calibrate(c,&i,0));
    for(int n=0;n<16;n++) { i=input(n*50000,1,0);assert(!bv_step(c,&i,n*50000)); }
    assert(c->state==BV_READY && c->baseline==1);
    assert(bv_arm(c,&i,750000));return 750000;
}
int main(void)
{
    bv_control_t c;int64_t t=ready(&c);bb_input_t i;
    for(int n=1;n<=20;n++) {
        t+=50000;i=input(t,5,1);bv_step(&c,&i,t);
        assert(c.target==4); /* raw5 - zero1 - deadband2 =2, gain2 =>4. */
    }
    assert(c.output==4 && bv_authorizes(&c,&i,4,t));
    i=input(t+1,1,1);assert(!bv_step(&c,&i,t+1) && c.output==0);
    for(int n=0;n<40;n++) { t+=50000;i=input(t,15,1);bv_step(&c,&i,t);assert(c.output<=8); }
    assert(c.output==8);i=input(t+1,15,21);bv_step(&c,&i,t+1);assert(c.output<2);
    i=input(t+2,15,22);assert(!bv_step(&c,&i,t+2) && c.state==BV_FAULT);
    i=input(t+3,1,0);assert(!bv_arm(&c,&i,t+3));
    t=ready(&c);i=input(t+1,4,1);bv_step(&c,&i,t+1);
    assert(!bv_authorizes(&c,&i,1,t+150001));
    i.torque.valid=false;assert(!bv_step(&c,&i,t+2) && c.reason==BV_DATA);
    t=ready(&c);i=input(t+1,4,1);i.brake=true;bv_step(&c,&i,t+1);assert(c.reason==BV_BRAKE);
    t=ready(&c);i=input(t+100001,4,1);bv_step(&c,&i,t+100001);assert(c.reason==BV_CLOCK);
    t=ready(&c);i=input(t+1,40,1);bv_step(&c,&i,t+1);assert(c.reason==BV_TORQUE_JUMP);
    t=ready(&c);i=input(t+1,65,1);bv_step(&c,&i,t+1);assert(c.reason==BV_RANGE);
    t=ready(&c);i=input(t+1,1,0);i.deadman=true;bv_step(&c,&i,t+1);assert(c.state==BV_IDLE);
    t=ready(&c);for(int n=0;n<80 && c.state==BV_ACTIVE;n++) {
        t+=50000;i=input(t,12,0);bv_step(&c,&i,t);
    }
    assert(c.state==BV_FAULT && c.reason==BV_STALL);
    bv_init(&c);i=input(0,9,0);assert(!bv_calibrate(&c,&i,0));
    i=input(0,1,0);assert(bv_calibrate(&c,&i,0));
    for(int n=0;n<30;n++) bv_step(&c,&i,0);
    assert(c.cal_count==1); /* duplicates cannot establish zero. */
    i=input(50000,5,0);bv_step(&c,&i,50000);assert(c.reason==BV_CAL_LOAD);
    return 0;
}
