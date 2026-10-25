/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
    bool extended;
    bool rtr;
    int64_t timestamp_us; /* Host read time, not an on-wire timestamp. */
} can_frame_t;

typedef enum { BIONX_QUERY, BIONX_SET, BIONX_REPLY, BIONX_UNKNOWN } bionx_packet_kind_t;
typedef struct {
    bionx_packet_kind_t kind;
    uint8_t reg;
    uint16_t raw; /* Preserve both bytes; no unvalidated unit or sign conversion. */
    uint8_t target; /* 0 for replies/unknown: CAN ID does not identify the sender. */
} bionx_packet_t;

bool can_frame_valid(const can_frame_t *frame);
bool bionx_parse(const can_frame_t *frame, bionx_packet_t *packet);
/* Only a candidate, even in the expected time window. No transaction ID. */
bool bionx_reply_candidate(const can_frame_t *frame, uint8_t expected_reg,
                           int64_t sent_us, int64_t deadline_us);

/* Register meanings from BionXtool f8cca39cb3cb8deab9f929d769c8209f23de8a4d.
 * Candidate interpretation only, not calibrated on this bicycle.
 * HI/LO entries are separate bytes, not a 16-bit value in one reply. */
typedef struct {
    const char *name;
    const char *unit; /* NULL: raw byte or part of a multi-register value. */
    float factor;
    bool signed_byte;
} bionx_register_info_t;

const bionx_register_info_t *bionx_register_info(uint8_t target, uint8_t reg);
/* Caller must supply a known target; never infer reply origin by register.
 * Refuses queries, unknown packets and nonzero high value bytes. */
bool bionx_value_candidate(const bionx_packet_t *packet, uint8_t target,
                           float *value, const char **unit);
