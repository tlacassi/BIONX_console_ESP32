/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "mcp2515_core.h"
/* Labels are controller status, never an inferred electrical root cause.
 * Raw registers and error code must accompany the label. */
static inline const char *mcp2515_tx_failure_name(const mcp2515_stats_t *s,esp_err_t err)
{
    if(s->spi_errors) return "SPI ERROR";
    if(s->bus_off || (s->eflg&0x20)) return "BUS OFF";
    if(s->mode_faults || s->mode) return "CAN MODE";
    if(err==ESP_ERR_TIMEOUT) return "TX TIMEOUT";
    if(s->last_tx_ctrl&0x10) return "TX BUS ERROR";
    if(s->last_tx_ctrl&0x20) return "ARBITRATION";
    if(s->last_tx_ctrl&0x40) return "TX ABORT";
    return "TX UNCONFIRMED";
}
