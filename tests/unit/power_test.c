/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include "bionx_master.h"
typedef struct { can_frame_t f; unsigned count, hi, lo, torque_reads; } bus_t;
static bool send(void *ctx,const can_frame_t *f)
{
    bus_t *b=ctx; b->f=*f; ++b->count;
    if(f->dlc==2 && f->id==0x20 && f->data[1]==0x21) ++b->torque_reads;
    if(f->dlc==4 && f->id==0x20 && f->data[1]==9) assert(!f->data[3]);
    return true;
}
static void simulate(bx_master_t *m,bus_t *b)
{
    for(int64_t t=1000;t<1000000;t+=1000) {
        unsigned before=b->count; bx_master_tick(m,t);
        if(b->count!=before && b->f.dlc==2) {
            can_frame_t r=b->f; r.id=8;r.dlc=4;r.timestamp_us=t+100;
            switch(r.data[1]) {
            case 0x21:r.data[3]=37;break;
            case 0xa6:r.data[3]=0x9a;break;
            case 0xa7:r.data[3]=0x4c;break;
            case 0x1e:r.data[3]=3;++b->hi;break;
            case 0x1f:r.data[3]=0xe8;++b->lo;break;
            default:r.data[3]=0;
            }
            bx_master_receive(m,&r);
        }
    }
}
/* Serialized delayed replies reproduce the runtime scheduling pressure. */
static void delayed_telemetry(int64_t latency,bool pedal)
{
    bx_config_t cfg=bx_default_config();cfg.fast_poll=true;cfg.measure_power=true;cfg.pedal_control=pedal;
    bx_master_t m;bus_t b={0};can_frame_t reply={0};int64_t due=0;
    assert(bx_master_init(&m,&cfg,send,&b));m.state=BX_NEUTRAL;
    unsigned measured=0;
    for(int64_t t=5000;t<5000000;t+=5000) {
        if(due && t>=due) { reply.timestamp_us=t;bx_master_receive(&m,&reply);due=0; }
        unsigned before=b.count;bx_master_tick(&m,t);
        if(b.count!=before && b.f.dlc==2) {
            reply=b.f;reply.id=8;reply.dlc=4;
            reply.data[3]=reply.data[1]==0xa6?0x9a:reply.data[1]==0xa7?0x4c:
                reply.data[1]==0x1e?3:reply.data[1]==0x1f?0xe8:0;
            due=t+latency;
        }
        if(t>1000000) {
            float w,a;
            assert(bx_sample_fresh(&m.samples[BX_SPEED],t,150000));
            if(pedal) assert(bx_sample_fresh(&m.samples[BX_TORQUE],t,150000));
            /* Publishing a new voltage pair alone cannot invalidate old U/I. */
            assert(bx_battery_power(&m,t,&w,&a));
            assert(w>39.49f && w<39.51f);++measured;
        }
    }
    assert(measured>700 && m.state==BX_NEUTRAL && b.torque_reads>=8);
}
int main(void)
{
    for(unsigned p=0;p<2;p++) {
        delayed_telemetry(1000,p);delayed_telemetry(6000,p);delayed_telemetry(11000,p);
    }
    bx_config_t cfg=bx_default_config();cfg.fast_poll=true;cfg.measure_power=true;
    bx_master_t m;bus_t b={0};float w,a;
    assert(bx_master_init(&m,&cfg,send,&b));m.state=BX_NEUTRAL;
    assert(!bx_battery_power(&m,0,&w,&a));
    simulate(&m,&b);assert(b.hi>3 && b.lo>3 && b.torque_reads>=3);
    assert(m.samples[BX_TORQUE].valid && m.samples[BX_TORQUE].raw==37);
    assert(bx_battery_power(&m,1000000,&w,&a) && w>39.49f && w<39.51f && a==1);
    assert(!bx_battery_power(&m,2000000,&w,&a));
    /* Pending HI must not corrupt the previous completed pair. */
    m.samples[BX_CURRENT_HI]=(bx_sample_t){.valid=true,.raw=0x7f,.timestamp_us=999999};
    assert(bx_battery_power(&m,1000000,&w,&a) && a==1);
    bx_sample_t save_hi=m.power_voltage_pair[0],save_lo=m.power_voltage_pair[1];
    m.power_voltage_pair[0].timestamp_us-=250000;m.power_voltage_pair[1].timestamp_us-=250000;
    assert(!bx_battery_power(&m,1000000,&w,&a)); /* Fresh U but unrelated to current. */
    m.power_voltage_pair[0]=save_hi;m.power_voltage_pair[1]=save_lo;
    bx_sample_t old=m.current_pair[0];
    m.current_pair[0].raw=0xfe;m.current_pair[1].raw=0x0c; /* -500mA */
    assert(bx_battery_power(&m,1000000,&w,&a) && w< -19.74f && w> -19.76f);
    m.current_pair[0]=old;m.current_pair[1].raw=0xe8;
    m.current_pair[1].timestamp_us=m.current_pair[0].timestamp_us-1;
    assert(!bx_battery_power(&m,1000000,&w,&a));
    m.current_pair[1].timestamp_us=m.current_pair[0].timestamp_us+100000;
    assert(!bx_battery_power(&m,1100000,&w,&a));
    m.state=BX_FAULT;assert(!bx_battery_power(&m,1000000,&w,&a));
    cfg.measure_power=false;b=(bus_t){0};
    assert(bx_master_init(&m,&cfg,send,&b));m.state=BX_NEUTRAL;
    simulate(&m,&b);assert(!b.hi && !b.lo && !bx_battery_power(&m,1000000,&w,&a));
    return 0;
}
