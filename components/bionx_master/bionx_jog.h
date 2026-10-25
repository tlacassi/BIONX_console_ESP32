/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "bionx_bench.h"
#define BJ_MAX_LEVEL 10
#define BJ_MAX_DRIVE 8
#define BJ_ROLLING_DRIVE 4
#define BJ_MAX_TARGET 20
#define BJ_OVERSPEED_MILLIKMH 25000U
typedef enum { BJ_IDLE, BJ_RUNNING, BJ_FAULT } bj_state_t;
typedef enum { BJ_OK, BJ_WAIT, BJ_CLICK, BJ_TIMEOUT, BJ_DATA, BJ_CLOCK, BJ_ENCODER, BJ_OVERSPEED, BJ_STALL } bj_reason_t;
typedef struct {
    bj_state_t state;
    bj_reason_t reason;
    uint8_t level, target, output; /* target in km/h; output in raw command units */
    uint16_t wheel_mm;
    bool ack_pending;
    int64_t last_us, ramp_us, lease_us, session_us, motion_us;
} bj_control_t;
void bj_init(bj_control_t *c);
/* No motion authorization until circumference is configured. */
bool bj_set_wheel(bj_control_t *c,uint16_t circumference_mm);
uint32_t bj_speed_millikmh(const bj_control_t *c,uint16_t raw);
void bj_stop(bj_control_t *c);
bool bj_reset(bj_control_t *c,const bb_input_t *i,int64_t now);
uint8_t bj_step(bj_control_t *c,const bb_input_t *i,int delta,bool clicked,bool bad_encoder,int64_t now);
bool bj_authorizes(const bj_control_t *c,const bb_input_t *i,uint8_t raw,int64_t now);
const char *bj_reason_name(bj_reason_t r);
