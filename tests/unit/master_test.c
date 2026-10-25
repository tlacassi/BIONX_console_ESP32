/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <math.h>
#include "bionx_master.h"
#include "bionx_control.h"
typedef struct { can_frame_t frame; unsigned count; bool fail; } bus_t;
static bool send(void *ctx,const can_frame_t *f)
{
    /* Check every emitted motor-assistance frame, across all states/scenarios. */
    if (f->id==0x20 && f->dlc==4 && f->data[1]==0x09)
        assert(f->data[0]==0 && f->data[2]==0 && f->data[3]==0);
    bus_t *b=ctx; b->frame=*f; ++b->count; return !b->fail;
}
static uint8_t answer(uint8_t reg)
{
    switch (reg) {
    case 0x3b: return 0x33; case 0x3c: return 0x69; case 0x3d: return 3;
    case 0x20: return 0x60; case 0xf9: return 0x23; case 0xfa: return 0x28;
    case 0xa6: return 0xc3; case 0xa7: return 0x50;
    case 0xfb: return 0xaf; case 0xfc: return 0xc8; default: return 0;
    }
}
static can_frame_t reply(bus_t *b)
{
    can_frame_t f=b->frame;
    f.id=8; f.dlc=4; f.data[3]=answer(f.data[1]); f.timestamp_us+=100;
    return f;
}
static void startup(bx_master_t *m,bus_t *b,int64_t *t)
{
    unsigned limit_writes=0;
    for (unsigned i=0;i<100 && m->state==BX_STARTING;++i) {
        unsigned old=b->count;
        bx_master_tick(m,*t);
        if (b->count!=old) {
            can_frame_t *f=&b->frame;
            if (f->dlc==2) { can_frame_t r=reply(b); bx_master_receive(m,&r); }
            else if (f->id==0x20 && f->data[1]>=0x7a && f->data[1]<=0x7d) {
                const uint8_t expected[]={0xaf,0xc8,0x23,0x28};
                assert(f->data[3]==expected[limit_writes++]);
            }
        }
        *t+=20000;
    }
    assert(m->state==BX_NEUTRAL && limit_writes==4);
}
int main(void)
{
    bx_master_t m; bus_t b={0}; bx_config_t cfg=bx_default_config(); int64_t t=0;
    assert(bx_master_init(&m,&cfg,send,&b));
    bx_master_tick(&m,1000000); assert(!b.count); /* Boot does not transmit. */
    assert(bx_master_start(&m,0)); startup(&m,&b,&t);
    for (unsigned i=0;i<100;++i) {
        unsigned old=b.count; bx_master_tick(&m,t);
        if (b.count!=old) {
            if (b.frame.dlc==2) { can_frame_t r=reply(&b); bx_master_receive(&m,&r); }
            else if (b.frame.id==0x20) assert(b.frame.data[1]==9 && !b.frame.data[3]);
        }
        t+=2000;
    }
    assert(m.state==BX_NEUTRAL);
    assert(m.voltage_pair[0].valid && m.voltage_pair[1].valid);
    /* Reading a new HI byte must preserve the previous complete pair. */
    m.samples[BX_PACK_HI]=(bx_sample_t){.raw=0xc4,.timestamp_us=t,.valid=true};
    float v;
    assert(bx_pack_voltage(&m,t+1000,&v) && fabsf(v-50)<0.001f);
    assert(!bx_pack_voltage(&m,t+cfg.sample_ttl_us+1000,&v));
    bx_master_stop(&m);
    const uint8_t regs[]={9,9,0x41,0x42,0x25};
    for (unsigned i=0;i<5;++i) { bx_master_tick(&m,t); assert(b.frame.data[1]==regs[i]); t+=20000; }
    bx_master_tick(&m,t); assert(m.state==BX_OFF && !bx_pack_voltage(&m,t,&v));
    assert(bx_master_start(&m,t));
    b.fail=true; bx_master_tick(&m,t); assert(m.state==BX_FAULT && m.fault==BX_TX_ERROR);
    bx_master_fault(&m,BX_RX_LOSS); assert(m.fault==BX_TX_ERROR);
    unsigned count=b.count; bx_master_tick(&m,t+1000000); assert(b.count==count);
    b.fail=false; assert(bx_master_start(&m,t));
    bx_master_tick(&m,t); t+=20000; bx_master_tick(&m,t); t+=20000; bx_master_tick(&m,t);
    assert(m.waiting);
    can_frame_t r=reply(&b); r.timestamp_us=t+cfg.reply_us;
    bx_master_receive(&m,&r); assert(m.waiting); /* Deadline exclusive. */
    bx_master_tick(&m,t+cfg.reply_us); assert(m.fault==BX_REPLY_TIMEOUT);
    assert(bx_master_start(&m,t));
    can_frame_t other={.id=0x20,.dlc=2,.data={0,0x11}};
    bx_master_receive(&m,&other); assert(m.fault==BX_OTHER_MASTER);
    assert(bx_master_start(&m,t));
    bx_master_tick(&m,t-1); assert(m.fault==BX_CLOCK);
    /* Wrong component and invalid limits must prevent motor unlock. */
    for (unsigned scenario=0;scenario<2;++scenario) {
        t=0; b=(bus_t){0}; assert(bx_master_init(&m,&cfg,send,&b)); assert(bx_master_start(&m,t));
        for (unsigned i=0;i<60 && m.state==BX_STARTING;++i) {
            unsigned old=b.count; bx_master_tick(&m,t);
            if (b.count!=old) {
                assert(b.frame.data[1]!=0xa5);
                if (b.frame.dlc==2) {
                    r=reply(&b);
                    if (!scenario && r.data[1]==0x3b) r.data[3]=0xff;
                    if (scenario && r.data[1]==0xf9) r.data[3]=0xff;
                    bx_master_receive(&m,&r);
                }
            }
            t+=20000;
        }
        assert(m.state==BX_FAULT && m.fault==(scenario?BX_BAD_LIMITS:BX_PROFILE_MISMATCH));
    }
    /* Wake discovery: no writes, failed sends/reply timeouts do not init. */
    b=(bus_t){.fail=true}; t=0;
    assert(bx_master_init(&m,&cfg,send,&b));
    assert(bx_master_discover(&m,t));
    bx_master_tick(&m,t); assert(b.count==1 && m.state==BX_WAIT_WAKE && !m.waiting);
    assert(b.frame.id==0x10 && b.frame.dlc==2 && b.frame.data[1]==0x3b);
    bx_master_tick(&m,999999); assert(b.count==1);
    b.fail=false; bx_master_tick(&m,1000000); assert(b.count==2 && m.waiting);
    r=reply(&b); r.data[1]=0x3c; bx_master_receive(&m,&r); assert(m.state==BX_WAIT_WAKE);
    r=reply(&b); r.timestamp_us=1200000; bx_master_receive(&m,&r); assert(m.state==BX_WAIT_WAKE);
    bx_master_tick(&m,1200000); assert(m.state==BX_WAIT_WAKE && !m.waiting);
    bx_master_tick(&m,2000000); assert(b.count==3 && m.waiting);
    r=reply(&b); bx_master_receive(&m,&r); assert(m.state==BX_STARTING);
    t=r.timestamp_us; startup(&m,&b,&t);
    bx_master_stop(&m);
    for (unsigned i=0;i<6;++i) { bx_master_tick(&m,t); t+=20000; }
    assert(m.state==BX_OFF);
    count=b.count; bx_master_tick(&m,t+5000000); assert(b.count==count);
    assert(bx_master_discover(&m,t)); bx_master_tick(&m,t);
    bx_master_stop(&m); count=b.count; bx_master_tick(&m,t+1);
    assert(m.state==BX_OFF && b.count==count); /* Waiting stop sends nothing. */
    assert(bx_master_discover(&m,t)); bx_master_tick(&m,t);
    r=reply(&b); r.data[3]=0xff; bx_master_receive(&m,&r);
    assert(m.state==BX_FAULT && m.fault==BX_PROFILE_MISMATCH);
    assert(bx_master_discover(&m,t)); bx_master_receive(&m,&other);
    assert(m.state==BX_FAULT && m.fault==BX_OTHER_MASTER);
    /* Controller is independently testable; no unvalidated input can arm. */
    bx_control_t c;
    bx_control_config_t cc={.max_assist=8,.max_regen=4,.ramp_step=1,.max_speed=25,
        .max_temperature=80,.min_voltage=30,.max_voltage=54,.max_discharge_a=10,
        .max_charge_a=2,.ttl_us=200000,.period_us=10000};
    assert(bx_control_init(&c,&cc));
    bx_control_input_t in={.brake_valid=true,.measurements_valid=true,.voltage=45,.temperature_c=25,
        .battery_charge_allowed=true,.battery_discharge_allowed=true,.rider_effort=64};
    assert(!bx_control_arm(&c,&in,0));
    in.profile_validated=true; assert(bx_control_arm(&c,&in,0));
    in.requested=64;
    for (int n=1;n<=12;++n) { in.timestamp_us=n*10000; assert(bx_control_step(&c,&in,in.timestamp_us)<=8); }
    assert(c.output==8);
    in.brake=true; assert(!bx_control_step(&c,&in,in.timestamp_us));
    in.brake=false; in.speed_kmh=26; in.timestamp_us+=10000;
    assert(!bx_control_step(&c,&in,in.timestamp_us));
    in.speed_kmh=10; in.requested=-64; in.battery_charge_allowed=false; in.timestamp_us+=10000;
    assert(!bx_control_step(&c,&in,in.timestamp_us));
    in.battery_charge_allowed=true;
    for(int n=0;n<10;++n) { in.timestamp_us+=10000; assert(bx_control_step(&c,&in,in.timestamp_us)>=-4); }
    assert(c.output==-4);
    assert(!bx_control_step(&c,&in,in.timestamp_us+cc.ttl_us));
    assert(c.state==BX_CONTROL_FAULT);
    in.requested=0; in.timestamp_us+=cc.ttl_us; assert(bx_control_arm(&c,&in,in.timestamp_us));
    in.voltage=NAN; assert(!bx_control_step(&c,&in,in.timestamp_us)); assert(c.state==BX_CONTROL_FAULT);
    return 0;
}
