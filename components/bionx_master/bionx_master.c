/* SPDX-License-Identifier: GPL-3.0-only */
#include <string.h>
#include "bionx_master.h"

/* Capture start&stop.csv, sequence only. No positive torque generation here. */
typedef struct { uint8_t node, reg, value; bool query; } op_t;
static const op_t startup[] = {
    {0x10,0x20,0,0}, {0x10,0x21,1,0},
    {0x10,0x3b,0,1}, {0x10,0x3c,0,1}, {0x10,0x22,0,0}, {0x10,0x3d,0,1},
    {0x20,0x02,0,0}, {0x20,0x41,0,0}, {0x20,0x42,1,0},
    {0x20,0x20,0,1}, {0x20,0x6c,0,1}, {0x20,0x11,0,1},
    {0x10,0xf9,0,1}, {0x10,0xfa,0,1}, {0x10,0xfb,0,1}, {0x10,0xfc,0,1},
    {0x20,0xa5,0xaa,0}, {0x20,0x7a,0,0}, {0x20,0x7b,0,0},
    {0x20,0x7c,0,0}, {0x20,0x7d,0,0},
};
static const op_t shutdown[] = {
    {0x20,0x09,0,0}, {0x20,0x09,0,0}, {0x20,0x41,0,0},
    {0x20,0x42,1,0}, {0x10,0x25,1,0},
};
static const op_t polls[] = {
    {0x20,0x21,0,1}, {0x20,0x11,0,1}, {0x20,0x14,0,1}, {0x20,0x16,0,1},
    {0x10,0x30,0,1}, {0x10,0x61,0,1}, {0x10,0xa6,0,1}, {0x10,0xa7,0,1},
    {0x10,0x1e,0,1}, {0x10,0x1f,0,1},
};
static const unsigned poll_samples[] = {
    BX_TORQUE, BX_SPEED, BX_POWER, BX_TEMP, BX_SOC_VOLTAGE, BX_SOC_GAUGE, BX_PACK_HI, BX_PACK_LO, BX_CURRENT_HI, BX_CURRENT_LO
};

/* Select one current sensor for the complete pair; never mix sources. */
static op_t current_poll(const bx_master_t *m,unsigned idx)
{
    op_t op=polls[idx];
    if(m->config.current_gas && idx>=8) op.reg=idx==8?0xd3:0xd4;
    return op;
}

bx_config_t bx_default_config(void)
{
    return (bx_config_t){.reply_us=200000, .write_gap_us=20000,
        .frame_gap_us=1000, .control_us=50000, .battery_us=250000, .sample_ttl_us=2000000,
        .max_charge_raw=9000, .max_discharge_raw=45000,
        .battery_hw=0x33, .battery_sw=0x69, .motor_sw=0x60};
}
bool bx_master_init(bx_master_t *m, const bx_config_t *c, bx_send_fn send, void *ctx)
{
    if (!m || !c || !send || c->reply_us < 1000 || c->reply_us > 1000000 ||
        c->frame_gap_us<1000 || c->frame_gap_us>10000 || c->write_gap_us < 1000 || c->write_gap_us > 200000 ||
        c->control_us < 10000 || c->control_us > 1000000 ||
        c->battery_us < c->control_us || c->battery_us > 2000000 ||
        c->sample_ttl_us < c->reply_us || c->sample_ttl_us > 10000000 ||
        (c->pedal_control && (!c->fast_poll || !c->measure_power)) ||
        c->assist_cap>BX_MAX_BENCH_ASSIST || (c->assist_cap && (!c->fast_poll || c->control_us>50000)) || !c->max_charge_raw || !c->max_discharge_raw) return false;
    *m=(bx_master_t){.config=*c,.send=send,.ctx=ctx};
    return true;
}
void bx_master_fault(bx_master_t *m, bx_fault_t fault)
{
    if (!m || m->state==BX_FAULT || fault==BX_OK) return;
    m->coast_left=m->last_assist && fault!=BX_TX_ERROR && fault!=BX_RX_LOSS && fault!=BX_OTHER_MASTER ? 2 : 0;
    m->assist_command=0;
    m->fault=fault; m->state=BX_FAULT; m->waiting=false;
    memset(m->samples,0,sizeof(m->samples));
    memset(m->voltage_pair,0,sizeof(m->voltage_pair));
    memset(m->current_pair,0,sizeof(m->current_pair));
    memset(m->power_voltage_pair,0,sizeof(m->power_voltage_pair));
}
bool bx_master_start(bx_master_t *m, int64_t now)
{
    if (!m || !m->send || now < 0 || now > INT64_MAX-10000000 ||
        (m->state != BX_OFF && m->state != BX_FAULT && m->state != BX_WAIT_WAKE)) return false;
    const bx_config_t c=m->config; bx_send_fn send=m->send; void *ctx=m->ctx;
    if (!bx_master_init(m,&c,send,ctx)) return false;
    m->state=BX_STARTING; m->now_us=m->next_us=now;
    return true;
}
bool bx_master_discover(bx_master_t *m, int64_t now)
{
    if (!m || (m->state!=BX_OFF && m->state!=BX_FAULT)) return false;
    if (!bx_master_start(m,now)) return false;
    m->state=BX_WAIT_WAKE;
    return true;
}
bool bx_master_set_assist(bx_master_t *m,uint8_t raw,int64_t now)
{
    if (!m || now<0 || now>INT64_MAX-10000000 || raw>m->config.assist_cap ||
        (raw && (m->state!=BX_NEUTRAL || m->stop_requested))) return false;
    if (!raw && m->assist_command) {
        m->zero_left=2; m->next_zero_us=now; m->next_us=now;
    }
    m->assist_command=raw; m->assist_deadline_us=now+100000;
    return true;
}
void bx_master_stop(bx_master_t *m) { if (m) m->stop_requested=true; }
static bool emit(bx_master_t *m, op_t op)
{
    can_frame_t f={.id=op.node,.dlc=op.query?2:4,.data={0,op.reg,0,op.value},.timestamp_us=m->now_us};
    if (op.query) {
        m->expected_reg=op.reg; m->expected_node=op.node; m->sent_us=m->now_us;
        m->deadline_us=m->now_us+m->config.reply_us; m->waiting=true;
    }
    if (!m->send(m->ctx,&f)) { bx_master_fault(m,BX_TX_ERROR); return false; }
    if (op.node==0x20 && op.reg==0x09 && !op.query) m->last_assist=op.value;
    m->next_us=m->now_us+(m->state==BX_NEUTRAL ? m->config.frame_gap_us : m->config.write_gap_us);
    return true;
}
void bx_master_receive(bx_master_t *m, const can_frame_t *f)
{
    if (!m || !f || m->state==BX_OFF || m->state==BX_FAULT) return;
    bionx_packet_t p;
    if (!bionx_parse(f,&p)) return;
    /* No local TX echo in normal MCP2515 mode: target-addressed frames are
     * evidence of another master. Unknown 0x58/009C alone is not attributed. */
    if (p.kind==BIONX_QUERY || p.kind==BIONX_SET) {
        bx_master_fault(m,BX_OTHER_MASTER); return;
    }
    if (!m->waiting || !bionx_reply_candidate(f,m->expected_reg,m->sent_us,m->deadline_us)) return;
    if (p.raw > 255) { bx_master_fault(m,BX_BAD_REPLY); return; }
    if (m->state==BX_WAIT_WAKE) {
        if (p.raw!=m->config.battery_hw) { bx_master_fault(m,BX_PROFILE_MISMATCH); return; }
        bx_master_start(m,f->timestamp_us);
        return;
    }
    if (m->state==BX_STARTING) {
        if ((m->step==2 && p.raw!=m->config.battery_hw) ||
            (m->step==3 && p.raw!=m->config.battery_sw) ||
            (m->step==5 && p.raw!=3) || (m->step==9 && p.raw!=m->config.motor_sw) ||
            (m->step==10 && p.raw!=0)) { bx_master_fault(m,BX_PROFILE_MISMATCH); return; }
        if (m->step>=12 && m->step<=15) m->limits[m->step-12]=p.raw;
        ++m->step;
    } else if (m->state==BX_NEUTRAL) {
        unsigned idx=poll_samples[m->poll_index];
        bool valid = !((idx==BX_SOC_VOLTAGE && p.raw>100) || (idx==BX_SOC_GAUGE && p.raw>15));
        m->samples[idx]=(bx_sample_t){.raw=p.raw,.timestamp_us=f->timestamp_us,.valid=valid};
        if (idx==BX_PACK_LO) {
            const bx_sample_t *hi=&m->samples[BX_PACK_HI];
            if (bx_sample_fresh(hi,f->timestamp_us,m->config.sample_ttl_us) &&
                f->timestamp_us-hi->timestamp_us<=m->config.reply_us*2) {
                m->voltage_pair[0]=*hi;
                m->voltage_pair[1]=m->samples[BX_PACK_LO];
            }
        }
        if (idx==BX_CURRENT_LO) {
            const bx_sample_t *hi=&m->samples[BX_CURRENT_HI];
            if (bx_sample_fresh(hi,f->timestamp_us,100000) &&
                hi->timestamp_us>m->current_pair[1].timestamp_us) {
                m->current_pair[0]=*hi;
                m->current_pair[1]=m->samples[BX_CURRENT_LO];
                m->power_voltage_pair[0]=m->voltage_pair[0];
                m->power_voltage_pair[1]=m->voltage_pair[1];
            } else memset(m->current_pair,0,sizeof(m->current_pair));
        }
        m->poll_index=(m->poll_index+1)%(m->config.measure_power?10:8);
        /* Bench telemetry needs about 1 Hz, not maximum bus throughput.
         * Leave the neutral and battery keepalive periods independent. */
        if (!m->poll_index) m->next_poll_us=f->timestamp_us+1000000;
    }
    m->waiting=false;
}
void bx_master_tick(bx_master_t *m, int64_t now)
{
    if (!m || m->state==BX_OFF) return;
    if (now<0 || now>INT64_MAX-10000000) { bx_master_fault(m,BX_CLOCK); return; }
    if (m->state==BX_FAULT) {
        if (m->coast_left && now>=m->next_us) {
            can_frame_t f={.id=0x20,.dlc=4,.data={0,0x09,0,0},.timestamp_us=now};
            if (m->send(m->ctx,&f)) { --m->coast_left; m->last_assist=0; }
            else m->coast_left=0;
            m->next_us=now+20000;
        }
        return;
    }
    if (now < m->now_us || now > INT64_MAX-10000000 ||
        (m->state==BX_NEUTRAL && now-m->now_us>m->config.sample_ttl_us)) { bx_master_fault(m,BX_CLOCK); return; }
    m->now_us=now;
    if (m->stop_requested && m->state==BX_WAIT_WAKE) {
        m->state=BX_OFF; m->waiting=false; return;
    }
    if (m->state==BX_WAIT_WAKE) {
        if (m->waiting && now>=m->deadline_us) m->waiting=false;
        if (now<m->next_us || m->waiting) return;
        /* No SET before a matching, timely HW-version reply. Failed one-shot
         * probes are expected while asleep; the app latches hard transport faults. */
        can_frame_t f={.id=0x10,.dlc=2,.data={0,0x3b},.timestamp_us=now};
        m->expected_node=0x10; m->expected_reg=0x3b; m->sent_us=now;
        m->deadline_us=now+m->config.reply_us; m->next_us=now+1000000;
        m->waiting=m->send(m->ctx,&f);
        return;
    }
    if (m->stop_requested && m->state!=BX_STOPPING) {
        m->assist_command=0;
        m->state=BX_STOPPING; m->step=0; m->waiting=false; m->next_us=now;
        memset(m->samples,0,sizeof(m->samples));
        memset(m->voltage_pair,0,sizeof(m->voltage_pair));
        memset(m->current_pair,0,sizeof(m->current_pair));
        memset(m->power_voltage_pair,0,sizeof(m->power_voltage_pair));
    }
    if (m->waiting && now>=m->deadline_us) { bx_master_fault(m,BX_REPLY_TIMEOUT); return; }
    if (now<m->next_us) return;
    if (m->state==BX_STOPPING) {
        if (m->step==sizeof(shutdown)/sizeof(shutdown[0])) { m->state=BX_OFF; return; }
        if (emit(m,shutdown[m->step])) ++m->step;
        return;
    }
    if (m->state==BX_STARTING) {
        if (m->waiting) return;
        if (m->step==sizeof(startup)/sizeof(startup[0])) {
            m->state=BX_NEUTRAL; m->next_zero_us=m->next_battery_us=now; return;
        }
        op_t op=startup[m->step];
        if (m->step==16) {
            uint16_t charge=(m->limits[0]<<8)|m->limits[1];
            uint16_t discharge=(m->limits[2]<<8)|m->limits[3];
            if (!charge || !discharge || charge>m->config.max_charge_raw || discharge>m->config.max_discharge_raw) {
                bx_master_fault(m,BX_BAD_LIMITS); return;
            }
        }
        if (m->step>=17 && m->step<=20) {
            static const unsigned map[]={2,3,0,1}; op.value=m->limits[map[m->step-17]];
        }
        if (emit(m,op) && !op.query) ++m->step;
        return;
    }
    /* Neutral only. Control output is independent of slow telemetry replies.
     * No catch-up bursts if a tick is late. */
    if (now>=m->next_zero_us && !m->zero_left) {
        m->zero_left=2; m->next_zero_us=now+m->config.control_us;
    }
    if (m->zero_left) {
        uint8_t cmd=now<m->assist_deadline_us ? m->assist_command : 0;
        if (emit(m,(op_t){0x20,0x09,cmd,0})) --m->zero_left;
        return;
    }
    if (now>=m->next_battery_us) {
        if (emit(m,(op_t){0x10,0x21,1,0})) m->next_battery_us=now+m->config.battery_us;
        return;
    }
    if (!m->waiting && m->config.fast_poll) {
        static const int64_t periods[]={50000,50000,50000,1000000,1000000,1000000,100000,100000,100000,100000};
        /* Jog displays pedal torque; keep speed priority and bound its read rate.
         * Speed takes priority over a saturated round-robin. Finish a started
         * HI/LO pair next, except when speed itself is due. */
        if(m->config.measure_power && now>=m->poll_due[1]) {
            m->poll_index=1; m->poll_due[1]=now+periods[1]; emit(m,current_poll(m,1)); return;
        }
        if(m->config.pedal_control && now>=m->poll_due[0]) {
            m->poll_index=0; m->poll_due[0]=now+50000; emit(m,current_poll(m,0)); return;
        }
        if(m->config.measure_power && m->pair_low) {
            unsigned low=m->pair_low;
            m->pair_low=low==7?8:low==8?9:0;
            m->poll_index=low;
            m->poll_due[low]=now+periods[low]; emit(m,current_poll(m,low)); return;
        }
        for (unsigned n=0;n<(m->config.measure_power?10U:8U);++n) {
            unsigned idx=((m->config.measure_power?m->poll_cursor:m->poll_index)+n)%(m->config.measure_power?10:8);
            if(m->config.measure_power && idx>=7) continue;
            if (now>=m->poll_due[idx]) {
                m->poll_index=idx;
                if(m->config.measure_power) {
                    m->poll_cursor=(idx+1)%10;
                    if(idx==6) m->pair_low=7;
                }
                m->poll_due[idx]=now+(m->config.measure_power && idx==0 ? (m->config.pedal_control?50000:250000) :
                    m->config.measure_power && idx==2 ? 250000 : periods[idx]);
                emit(m,current_poll(m,idx));
                break;
            }
        }
    } else if (!m->waiting && now>=m->next_poll_us) emit(m,current_poll(m,m->poll_index));
}
bool bx_sample_fresh(const bx_sample_t *s,int64_t now,int64_t ttl)
{ return s && s->valid && ttl>0 && s->timestamp_us>=0 && now>=s->timestamp_us && now-s->timestamp_us<ttl; }
bool bx_pack_voltage(const bx_master_t *m,int64_t now,float *v)
{
    if (!m || !v || m->state!=BX_NEUTRAL) return false;
    const bx_sample_t *hi=&m->voltage_pair[0], *lo=&m->voltage_pair[1];
    if (!bx_sample_fresh(hi,now,m->config.sample_ttl_us) ||
        !bx_sample_fresh(lo,now,m->config.sample_ttl_us) ||
        lo->timestamp_us<hi->timestamp_us || lo->timestamp_us-hi->timestamp_us>m->config.reply_us*2) return false;
    *v=((hi->raw<<8)|lo->raw)*0.001f; return *v>0;
}

static bool electrical_pair(const bx_sample_t pair[2],int64_t now)
{
    return bx_sample_fresh(&pair[0],now,500000) && bx_sample_fresh(&pair[1],now,500000) &&
        pair[0].raw<=255 && pair[1].raw<=255 &&
        pair[1].timestamp_us>=pair[0].timestamp_us &&
        pair[1].timestamp_us-pair[0].timestamp_us<100000;
}
bool bx_battery_power(const bx_master_t *m,int64_t now,float *watts,float *amps)
{
    if(!m || !watts || !amps || !m->config.measure_power || m->state!=BX_NEUTRAL ||
        !electrical_pair(m->power_voltage_pair,now) || !electrical_pair(m->current_pair,now)) return false;
    int64_t skew=m->power_voltage_pair[1].timestamp_us-m->current_pair[1].timestamp_us;
    if(skew < -200000 || skew > 200000) return false;
    unsigned mv=(m->power_voltage_pair[0].raw<<8)|m->power_voltage_pair[1].raw;
    unsigned raw=(m->current_pair[0].raw<<8)|m->current_pair[1].raw;
    int ma=raw>=32768 ? (int)raw-65536 : (int)raw;
    if(mv<30000 || mv>42000 || ma == -32768 || ma == 32767) return false;
    *amps=ma*(m->config.current_gas ? 0.002141f:0.001f);
    *watts=mv*0.001f * *amps;
    return true;
}
