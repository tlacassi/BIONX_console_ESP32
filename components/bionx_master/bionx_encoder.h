/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Caller serializes ISR/task access. No allocation, logging or GPIO dependency. */
#define BX_ENCODER_QUEUE 16
typedef struct {
    unsigned previous, count;
    int partial;
    int8_t steps[BX_ENCODER_QUEUE];
    bool bad, stop, pressed;
    uint32_t invalid, overflow;
} bx_encoder_t;
typedef struct {
    unsigned count;
    int8_t steps[BX_ENCODER_QUEUE];
    bool bad, stop;
} bx_encoder_batch_t;
static inline void bx_encoder_init(bx_encoder_t *e,unsigned ab,bool pressed)
{ *e=(bx_encoder_t){.previous=ab&3,.pressed=pressed,.stop=pressed}; }
static inline void bx_encoder_button(bx_encoder_t *e,bool pressed)
{
    e->pressed=pressed;
    if(pressed) { e->stop=true; e->count=0; e->partial=0; }
}
static inline void bx_encoder_edge(bx_encoder_t *e,unsigned ab)
{
    static const int8_t table[]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    ab&=3;
    if((ab^e->previous)==3) {
        ++e->invalid; e->bad=true; e->count=0; e->partial=0;
    } else if(!e->bad && !e->stop && !e->pressed) {
        e->partial+=table[(e->previous<<2)|ab];
        int delta=e->partial>=4?1:e->partial<=-4?-1:0;
        if(delta) {
            e->partial=0;
            if(e->count==BX_ENCODER_QUEUE) { ++e->overflow; e->bad=true; e->count=0; }
            else e->steps[e->count++]=delta;
        }
    } else e->partial=0;
    e->previous=ab;
}
static inline bx_encoder_batch_t bx_encoder_take(bx_encoder_t *e)
{
    bx_encoder_batch_t b={.count=e->count,.bad=e->bad,.stop=e->stop};
    for(unsigned n=0;n<b.count;++n) b.steps[n]=e->steps[n];
    e->count=0; e->bad=false; e->stop=false;
    if(b.bad || b.stop) e->partial=0;
    return b;
}
