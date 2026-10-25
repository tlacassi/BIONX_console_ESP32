/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#define BX_MAX_BENCH_ASSIST 8
#include <stddef.h>
#include "bionx_protocol.h"

typedef enum { BX_OFF, BX_STARTING, BX_NEUTRAL, BX_STOPPING, BX_FAULT, BX_WAIT_WAKE } bx_state_t;
typedef enum { BX_OK, BX_TX_ERROR, BX_REPLY_TIMEOUT, BX_PROFILE_MISMATCH,
    BX_BAD_LIMITS, BX_OTHER_MASTER, BX_RX_LOSS, BX_CLOCK, BX_BAD_REPLY } bx_fault_t;
typedef struct {
    int64_t reply_us, write_gap_us, frame_gap_us, control_us, battery_us, sample_ttl_us;
    uint16_t max_charge_raw, max_discharge_raw;
    bool fast_poll, measure_power, pedal_control, current_gas;
    uint8_t assist_cap; /* 0 in the normal neutral profile; bench maximum 8. */
    uint8_t battery_hw, battery_sw, motor_sw;
} bx_config_t;
typedef struct { uint16_t raw; int64_t timestamp_us; bool valid; } bx_sample_t;
enum { BX_SPEED, BX_TORQUE, BX_POWER, BX_TEMP, BX_SOC_VOLTAGE, BX_SOC_GAUGE,
       BX_PACK_HI, BX_PACK_LO, BX_CURRENT_HI, BX_CURRENT_LO, BX_SAMPLE_COUNT };
typedef bool (*bx_send_fn)(void *ctx, const can_frame_t *frame);
typedef struct {
    bx_config_t config;
    bx_state_t state;
    bx_fault_t fault;
    bx_send_fn send;
    void *ctx;
    unsigned step, poll_index, poll_cursor, pair_low, zero_left, coast_left;
    uint8_t assist_command, last_assist;
    int64_t assist_deadline_us, poll_due[BX_SAMPLE_COUNT];
    uint8_t limits[4], expected_reg, expected_node;
    bool waiting, stop_requested;
    int64_t now_us, sent_us, deadline_us, next_us, next_zero_us, next_battery_us, next_poll_us;
    bx_sample_t samples[BX_SAMPLE_COUNT];
    bx_sample_t current_pair[2], power_voltage_pair[2];
    bx_sample_t voltage_pair[2]; /* Last complete HI/LO pair, not half-updated. */
} bx_master_t;

/* Defaults are timing hypotheses, not timing extracted from the CSV. */
bx_config_t bx_default_config(void);
bool bx_master_init(bx_master_t *m, const bx_config_t *cfg, bx_send_fn send, void *ctx);
/* Explicit start/rearm; never automatic after a fault. Electrical wake external. */
bool bx_master_start(bx_master_t *m, int64_t now_us);
/* Arm discovery: one HW-version query per second, then validated startup. */
bool bx_master_discover(bx_master_t *m, int64_t now_us);
/* Ephemeral positive-only bench command, expires after 100ms. */
bool bx_master_set_assist(bx_master_t *m, uint8_t raw, int64_t now_us);
void bx_master_stop(bx_master_t *m);
void bx_master_fault(bx_master_t *m, bx_fault_t fault);
void bx_master_receive(bx_master_t *m, const can_frame_t *frame);
void bx_master_tick(bx_master_t *m, int64_t now_us);
bool bx_sample_fresh(const bx_sample_t *s, int64_t now_us, int64_t ttl_us);
/* Pair needs both fresh adjacent samples, HI first, bounded skew. */
bool bx_pack_voltage(const bx_master_t *m, int64_t now_us, float *volts);

/* Electrical battery power, signed sensor current, fresh paired U/I only.
 * Sensor calibration/sign/path must be checked on the actual pack. */
bool bx_battery_power(const bx_master_t *m, int64_t now_us, float *watts, float *amps);
