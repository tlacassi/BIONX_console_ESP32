/* SPDX-License-Identifier: GPL-3.0-only */
#include <stddef.h>
#include "bionx_protocol.h"

bool can_frame_valid(const can_frame_t *f)
{
    return f && f->dlc <= 8 && f->timestamp_us >= 0 &&
        f->id <= (f->extended ? 0x1fffffffU : 0x7ffU);
}

bool bionx_parse(const can_frame_t *f, bionx_packet_t *packet)
{
    const bool reply = f && (f->id == 0x08 || f->id == 0x58);
    const bool target = f && (f->id == 0x10 || f->id == 0x20 ||
        f->id == 0x48 || f->id == 0x50 || f->id == 0x60);
    if (!packet || !can_frame_valid(f) || f->extended || f->rtr ||
        (!reply && !target) ||
        (f->dlc != 2 && f->dlc != 4) || f->data[0] != 0 ||
        (f->id == 0x08 && f->dlc != 4)) return false;
    *packet = (bionx_packet_t){
        /* 0x58 DLC 2 is observed but its role is not established. */
        .kind = reply ? (f->dlc == 4 ? BIONX_REPLY : BIONX_UNKNOWN) :
            f->dlc == 2 ? BIONX_QUERY : BIONX_SET,
        .reg = f->data[1],
        .raw = f->dlc == 4 ? (uint16_t)((f->data[2] << 8) | f->data[3]) : 0,
        .target = reply ? 0 : (uint8_t)f->id,
    };
    return true;
}

bool bionx_reply_candidate(const can_frame_t *f, uint8_t reg,
                           int64_t sent_us, int64_t deadline_us)
{
    bionx_packet_t packet;
    return sent_us >= 0 && deadline_us > sent_us && bionx_parse(f, &packet) &&
        f->id == 0x08 && packet.kind == BIONX_REPLY && packet.reg == reg &&
        f->timestamp_us >= sent_us && f->timestamp_us < deadline_us;
}

/* Names and candidate factors independently described from BionXtool's
 * register documentation; source pinned in BIONXTOOL_ANALYSIS.md. */
typedef struct {
    uint8_t target, reg;
    bionx_register_info_t info;
} register_entry_t;
static const register_entry_t registers[] = {
    {0x20, 0x09, {"assist", "%", 1.5625f, true}},
    {0x20, 0x11, {"speed", "rpm", 9.091f, false}},
    {0x20, 0x12, {"revolutions_read_reset", NULL, 0, false}},
    {0x20, 0x14, {"power_meter", "%", 1.5625f, false}},
    {0x20, 0x16, {"temperature", "C", 1, false}},
    {0x20, 0x21, {"torque_gauge", "%", 1.5625f, false}},
    {0x20, 0x41, {"limit_3kmh_unconfirmed", NULL, 0, false}},
    {0x20, 0x42, {"direction", NULL, 0, false}},
    {0x20, 0x72, {"supply_12v_hi", NULL, 0, false}},
    {0x20, 0x73, {"supply_12v_lo", NULL, 0, false}},
    {0x10, 0x21, {"power_enable", NULL, 0, false}},
    {0x10, 0x25, {"shutdown", NULL, 0, false}},
    {0x10, 0x30, {"soc_voltage_estimate", "%", 1, false}},
    {0x10, 0x3b, {"hardware_revision", NULL, 0, false}},
    {0x10, 0x3c, {"software_revision", NULL, 0, false}},
    {0x10, 0x61, {"charge_gauge", "%", 100.0f / 15.0f, false}},
    {0x10, 0xbc, {"soc_gas_gauge", "%", 1, false}},
    {0x10, 0xa6, {"pack_voltage_hi", NULL, 0, false}},
    {0x10, 0xa7, {"pack_voltage_lo", NULL, 0, false}},
    {0x10, 0xd3, {"gas_current_hi", NULL, 0, false}},
    {0x10, 0xd4, {"gas_current_lo", NULL, 0, false}},
    {0x10, 0xd5, {"capacity_lmd_hi", NULL, 0, false}},
    {0x10, 0xd6, {"capacity_lmd_lo", NULL, 0, false}},
};

const bionx_register_info_t *bionx_register_info(uint8_t target, uint8_t reg)
{
    if (target == 0x50) target = 0x10;
    if (target == 0x60) target = 0x20;
    for (size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i)
        if (registers[i].target == target && registers[i].reg == reg)
            return &registers[i].info;
    return NULL;
}

bool bionx_value_candidate(const bionx_packet_t *p, uint8_t target,
                           float *value, const char **unit)
{
    if (!p || !value || !unit || p->raw > 0xff ||
        (p->kind != BIONX_REPLY && p->kind != BIONX_SET) ||
        (p->kind == BIONX_SET && p->target != target)) return false;
    const bionx_register_info_t *info = bionx_register_info(target, p->reg);
    if (!info || !info->unit) return false;
    /* Explicit arithmetic sign extension, independent of char signedness. */
    int raw = p->raw;
    if (info->signed_byte && raw >= 128) raw -= 256;
    if (info->unit[0] == '%' &&
        (raw * info->factor > 100.01f || raw * info->factor < -100.01f)) return false;
    *value = raw * info->factor;
    *unit = info->unit;
    return true;
}
