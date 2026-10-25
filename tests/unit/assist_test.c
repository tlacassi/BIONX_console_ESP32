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
        assert(c.target==6); /* raw5 - zero1 - deadband1 =3, gain2 =>6. */
    }
    assert(c.output==6 && bv_authorizes(&c,&i,6,t));
    i=input(t+1,1,1);assert(bv_step(&c,&i,t+1)==6);
    for(int n=1;n<=6;n++) {
        i=input(t+n*BV_DECAY_US,1,1);bv_step(&c,&i,t+n*BV_DECAY_US);
        assert(c.output==6-n);
    }
    t+=6*BV_DECAY_US;assert(!c.output);
    for(int n=0;n<40;n++) { t+=50000;i=input(t,15,1);bv_step(&c,&i,t);assert(c.output<=12); }
    assert(c.output==12);i=input(t+1,15,21);bv_step(&c,&i,t+1);assert(c.output==12 && bv_authorizes(&c,&i,12,t+1));
    i=input(t+2,15,22);assert(bv_step(&c,&i,t+2)==12 && c.state==BV_ACTIVE);
    assert(bv_authorizes(&c,&i,12,t+2));
    i=input(t+3,15,30);assert(bv_step(&c,&i,t+3)==12 && c.state==BV_ACTIVE);
    i=input(t+4,15,31);assert(!bv_step(&c,&i,t+4) && c.state==BV_ACTIVE && c.speed_limited);
    assert(!bv_authorizes(&c,&i,12,t+4) && !c.pulse_peak);
    i=input(t+5,1,29);assert(!bv_step(&c,&i,t+5) && !c.speed_limited);
    i=input(t+6,15,255);assert(!bv_step(&c,&i,t+6) && c.state==BV_ACTIVE);
    i=input(t+7,15,256);assert(!bv_step(&c,&i,t+7) && c.state==BV_FAULT);
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
    /* Fully automatic boot: no positive command during zeroing/arm. */
    bv_init(&c);
    for(int n=0;n<40;n++) {
        t=n*50000;i=input(t,1,0);
        assert(!bv_auto_step(&c,&i,t));
    }
    assert(c.state==BV_ACTIVE && !c.boot_pending && c.baseline==1);
    for(int n=0;n<20;n++) { t+=50000;i=input(t,5,1);bv_auto_step(&c,&i,t); }
    assert(c.output==6);
    bv_stop(&c);
    for(int n=0;n<40;n++) {t+=50000;i=input(t,1,0);assert(!bv_auto_step(&c,&i,t));}
    assert(c.state==BV_IDLE && !c.boot_pending); /* no automatic restart after stop */
    bv_init(&c);
    for(int n=0;n<30;n++) {i=input(n*50000,10,0);assert(!bv_auto_step(&c,&i,n*50000));}
    assert(c.state==BV_IDLE && !c.calibrated); /* loaded pedal cannot calibrate */
    bv_init(&c);
    for(int n=0;n<30;n++) {i=input(n*50000,n%2?4:0,0);assert(!bv_auto_step(&c,&i,n*50000));}
    assert(!c.calibrated); /* unstable zero restarts observation */
    bv_init(&c);
    for(int n=0;n<40;n++) {t=n*50000;i=input(t,0,0);bv_auto_step(&c,&i,t);}
    assert(c.state==BV_ACTIVE);
    i=input(t+50000,40,0);bv_auto_step(&c,&i,t+50000);assert(c.state==BV_FAULT);
    for(int n=2;n<40;n++){i=input(t+n*50000,0,0);assert(!bv_auto_step(&c,&i,t+n*50000));}
    assert(c.state==BV_FAULT); /* no recovery after runtime fault */
    bv_init(&c);i=input(0,0,0);i.deadman=true;bv_auto_step(&c,&i,0);
    i=input(50000,0,0);bv_auto_step(&c,&i,50000);assert(c.state==BV_IDLE && !c.boot_pending);
    /* A brief fresh pulse starts a bounded response even after effort drops. */
    t=ready(&c);i=input(t+5000,14,0);bv_step(&c,&i,t+5000);
    bool response=false,extended=false;
    for(int n=1;n<=36;n++) {
        int64_t now=t+5000+n*25000;i=input(now,1,1);
        uint8_t cmd=bv_step(&c,&i,now);assert(cmd<=12);
        if(n>=20 && cmd) extended=true;
        if(cmd) { response=true;assert(bv_authorizes(&c,&i,cmd,now)); }
        if(n>=32) assert(!cmd); /* tail expires by800ms */
    }
    assert(response && extended);
    t=ready(&c);i=input(t+5000,14,0);bv_step(&c,&i,t+5000);
    i=input(t+55000,1,1);assert(bv_step(&c,&i,t+55000)>0);
    i.torque.valid=false;assert(!bv_step(&c,&i,t+55001));
    assert(c.state==BV_FAULT && !c.pulse_peak);
    t=ready(&c);i=input(t+5000,14,0);bv_step(&c,&i,t+5000);
    i=input(t+55000,1,1);bv_step(&c,&i,t+55000);
    i.deadman=true;assert(!bv_step(&c,&i,t+55001) && !c.pulse_peak);
    t=ready(&c);
    for(int n=0;n<4;n++){t+=25000;i=input(t,2,1);assert(!bv_step(&c,&i,t));}
    for(int n=0;n<4;n++){t+=25000;i=input(t,3,1);bv_step(&c,&i,t);}
    assert(c.output==2); /* only two raw points above baseline now produces assist */
    t=ready(&c);bv_button_t button={.button={.armed=true}};
    i=input(t+1,1,0);i.deadman=true;bv_button_step(&c,&button,&i,t+1);
    assert(c.state==BV_IDLE && !c.boot_pending);
    i=input(t+30001,1,0);i.deadman=true;bv_button_step(&c,&button,&i,t+30001);
    i=input(t+60001,1,0);bv_button_step(&c,&button,&i,t+60001);
    i=input(t+90001,1,0);bv_button_step(&c,&button,&i,t+90001);
    assert(c.state==BV_IDLE && !c.boot_pending); /* release does not restart */
    bv_control_t moving=c;bv_button_t refused=button;
    for(int n=0;n<4;n++) {
        int64_t now=t+120001+n*30000;i=input(now,1,1);i.deadman=n<2;
        bv_button_step(&moving,&refused,&i,now);
    }
    assert(moving.state==BV_IDLE && !moving.boot_pending);
    i=input(t+240001,1,0);bv_button_step(&moving,&refused,&i,t+240001);
    assert(moving.state==BV_IDLE && !moving.boot_pending); /* no deferred resume */
    i=input(t+120001,1,0);i.deadman=true;bv_button_step(&c,&button,&i,t+120001);
    i=input(t+150001,1,0);i.deadman=true;bv_button_step(&c,&button,&i,t+150001);
    i=input(t+180001,1,0);bv_button_step(&c,&button,&i,t+180001);
    i=input(t+210001,1,0);bv_button_step(&c,&button,&i,t+210001);
    assert(c.state==BV_CALIBRATING && c.boot_pending && !c.output);
    return 0;
}
