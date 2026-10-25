/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "bionx_master.h"
typedef enum { BB_IDLE, BB_CALIBRATING, BB_READY, BB_RUNNING, BB_FAULT } bb_state_t;
typedef enum { BB_OK, BB_NOT_READY, BB_STALE, BB_RANGE, BB_BRAKE, BB_CLOCK,
    BB_TORQUE_JUMP, BB_CAL_LOAD, BB_RELEASED, BB_FINISHED, BB_ARM_TIMEOUT } bb_reason_t;
typedef struct {
    bool healthy, deadman, brake_valid, brake;
    bx_sample_t torque, speed, temperature, voltage_mv, power;
} bb_input_t;
typedef struct {
    bb_state_t state;
    bb_reason_t reason;
    bool brake_required, calibrated;
    uint8_t baseline, cal_min, cal_max, last_torque, output;
    unsigned cal_count;
    int64_t last_us, torque_us, ramp_us, deadline_us;
} bb_control_t;
/* Bench only: raw demand 0..4, no regen, 800ms maximum per explicit arm.
 * This calculates a proposal. Hardware TX permission belongs to the app. */
void bb_init(bb_control_t *c, bool brake_required);
bool bb_calibrate(bb_control_t *c, const bb_input_t *i, int64_t now);
bool bb_arm(bb_control_t *c, const bb_input_t *i, int64_t now);
void bb_disarm(bb_control_t *c);
uint8_t bb_step(bb_control_t *c, const bb_input_t *i, int64_t now);
bool bb_authorizes(const bb_control_t *c,const bb_input_t *i,uint8_t raw,int64_t now);
const char *bb_state_name(bb_state_t s);
const char *bb_reason_name(bb_reason_t r);
