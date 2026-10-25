/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "bionx_bench.h"
#include "bionx_button.h"
/* Experimental V1, raw sensor gain, not a calibrated torque ratio. */
#define BV_MAX_DRIVE 12
#define BV_DEADBAND 1
#define BV_ATTACK_US 25000
#define BV_DECAY_US 66667
#define BV_PULSE_US 800000
#define BV_MAX_SPEED_KMH 35
#define BV_TORQUE_TTL 150000
#define BV_SPEED_TTL 150000
#define BV_COMMAND_TTL 100000
#define BV_WHEEL_MM 2136

typedef enum { BV_IDLE, BV_CALIBRATING, BV_READY, BV_ACTIVE, BV_FAULT } bv_state_t;
typedef enum { BV_OK, BV_DATA, BV_RANGE, BV_CLOCK, BV_CAL_LOAD, BV_TORQUE_JUMP,
    BV_STOPPED, BV_BRAKE, BV_STALL } bv_reason_t;
typedef struct {
    bv_state_t state;
    bv_reason_t reason;
    bool calibrated, boot_pending, speed_limited;
    uint8_t baseline, cal_min, cal_max, last_torque, output, target, pulse_peak;
    unsigned cal_count;
    int64_t last_us, torque_us, ramp_us, stalled_since, pulse_us;
} bv_control_t;
void bv_init(bv_control_t *c);
bool bv_calibrate(bv_control_t *c,const bb_input_t *i,int64_t now);
bool bv_arm(bv_control_t *c,const bb_input_t *i,int64_t now);
void bv_stop(bv_control_t *c);
uint8_t bv_auto_step(bv_control_t *c,const bb_input_t *i,int64_t now);
uint8_t bv_step(bv_control_t *c,const bb_input_t *i,int64_t now);
bool bv_authorizes(const bv_control_t *c,const bb_input_t *i,uint8_t raw,int64_t now);
uint32_t bv_speed_millikmh(uint16_t raw);
const char *bv_state_name(bv_state_t s);
const char *bv_reason_name(bv_reason_t r);

typedef struct { bx_button_t button; bool stopped_this_press; } bv_button_t;
void bv_button_step(bv_control_t *c,bv_button_t *b,const bb_input_t *i,int64_t now);
const char *bv_status_name(const bv_control_t *c);
