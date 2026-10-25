/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include "bionx_encoder.h"
#include "bionx_jog.h"
static void turn(bx_encoder_t *e,bool positive)
{
    const unsigned pos[]={2,3,1,0},neg[]={1,3,2,0};
    for(unsigned n=0;n<4;++n) bx_encoder_edge(e,positive?pos[n]:neg[n]);
}
int main(void)
{
    bx_encoder_t e; bx_encoder_batch_t b;
    bx_encoder_init(&e,0,false);
    bx_encoder_edge(&e,2); bx_encoder_edge(&e,0); /* Contact bounce/reversal cancels. */
    turn(&e,true); b=bx_encoder_take(&e);
    assert(b.count==1 && b.steps[0]==1 && !b.bad && !b.stop);
    /* Slow consumer does not miss valid edges: three turns between task ticks. */
    turn(&e,true);turn(&e,true);turn(&e,false); b=bx_encoder_take(&e);
    assert(b.count==3 && b.steps[0]==1 && b.steps[1]==1 && b.steps[2]==-1 && !b.bad);
    /* Partial cycles persist across consumer ticks. */
    bx_encoder_edge(&e,2); b=bx_encoder_take(&e); assert(!b.count);
    bx_encoder_edge(&e,3); bx_encoder_edge(&e,1); bx_encoder_edge(&e,0);
    b=bx_encoder_take(&e); assert(b.count==1 && b.steps[0]==1);
    turn(&e,true);
    bx_encoder_button(&e,true); bx_encoder_button(&e,false); /* Brief completed click is latched. */
    turn(&e,true); b=bx_encoder_take(&e); assert(b.stop && !b.count);
    turn(&e,true); b=bx_encoder_take(&e); assert(b.count==1 && !b.stop);
    bx_encoder_edge(&e,3); b=bx_encoder_take(&e); assert(b.bad && !b.count && e.invalid==1);
    bx_encoder_init(&e,0,false);
    for(unsigned n=0;n<=BX_ENCODER_QUEUE;++n) turn(&e,true);
    b=bx_encoder_take(&e); assert(b.bad && !b.count && e.overflow==1);
    bx_encoder_init(&e,0,true); turn(&e,true); b=bx_encoder_take(&e);
    assert(b.stop && !b.count); turn(&e,true); b=bx_encoder_take(&e); assert(!b.count);
    /* Reproduces the old polling failure: 00 -> 11 looked invalid, although
     * the ISR observes the actual intermediate 10 edge. */
    bx_encoder_init(&e,0,false); bx_encoder_edge(&e,2); bx_encoder_edge(&e,3);
    bx_encoder_edge(&e,1); bx_encoder_edge(&e,0); b=bx_encoder_take(&e);
    assert(!b.bad && b.count==1);
    bb_input_t i={.healthy=true,.speed={.valid=true},.power={.valid=true},
        .temperature={.valid=true,.raw=36},.voltage_mv={.valid=true,.raw=39500}};
    bj_control_t c; bj_init(&c); assert(bj_set_wheel(&c,2136));
    bx_encoder_init(&e,0,false);turn(&e,true);turn(&e,true);b=bx_encoder_take(&e);
    for(unsigned n=0;n<b.count;++n) bj_step(&c,&i,b.steps[n],false,b.bad,0);
    assert(c.state==BJ_RUNNING && c.level==2 && c.target==4);
    bx_encoder_button(&e,true);bx_encoder_button(&e,false);b=bx_encoder_take(&e);
    assert(!bj_step(&c,&i,0,b.stop,b.bad,0) && c.state==BJ_IDLE && !c.target);
    return 0;
}
