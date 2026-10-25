/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "bionx_master.h"
typedef enum { BX_CONTROL_DISARMED, BX_CONTROL_ARMED, BX_CONTROL_FAULT } bx_control_state_t;
typedef struct {
    bool brake_valid, brake, measurements_valid, battery_discharge_allowed, battery_charge_allowed;
    bool profile_validated; /* Includes brake wiring and motor timeout measured on bench. */
    int64_t timestamp_us;
    float speed_kmh, temperature_c, voltage, current_a;
    uint8_t rider_effort; /* Calibrated demand 0..64, never an unvalidated sensor byte. */
    int8_t requested; /* Signed raw byte -64..64. */
} bx_control_input_t;
typedef struct {
    int max_assist, max_regen, ramp_step;
    float max_speed, max_temperature, min_voltage, max_voltage, max_discharge_a, max_charge_a;
    int64_t ttl_us, period_us;
} bx_control_config_t;
typedef struct {
    bx_control_state_t state;
    bx_control_config_t config;
    int output;
    int64_t last_us;
} bx_control_t;
bool bx_control_init(bx_control_t *c, const bx_control_config_t *config);
bool bx_control_arm(bx_control_t *c, const bx_control_input_t *input, int64_t now);
/* Immediate downward limiting, bounded upward ramp; brake inhibits positive torque.
 * Faults require explicit rearm with zero request and brake released. */
int8_t bx_control_step(bx_control_t *c,const bx_control_input_t *input,int64_t now);
