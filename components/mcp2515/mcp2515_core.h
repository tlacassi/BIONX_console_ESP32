/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include "esp_err.h"
#include "bionx_protocol.h"

typedef struct {
    void *ctx;
    esp_err_t (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    void (*delay_ms)(void *ctx, unsigned ms);
    int64_t (*now_us)(void *ctx);
} mcp2515_io_t;

typedef struct {
    uint32_t received, malformed, overflow0_events, overflow1_events;
    uint32_t spi_errors, mode_faults;
    uint32_t transmitted, tx_errors, tx_timeouts, bus_off;
    uint8_t eflg, tec, rec, mode, last_tx_ctrl, last_tx_intf;
    bool ready;
} mcp2515_stats_t;

typedef struct {
    mcp2515_io_t io;
    mcp2515_stats_t stats;
    bool active, tx_pending, discovery;
    int64_t next_health_us;
} mcp2515_t;

/* Passive init never enables transmission. Active init is explicit, one-shot. */
esp_err_t mcp2515_init_passive(mcp2515_t *dev, const mcp2515_io_t *io, unsigned osc_mhz);
/* One bounded RX read. NOT_FOUND=no frame; INVALID_RESPONSE=malformed frame
 * consumed; other errors latch ready=false. Caller serializes access. */
esp_err_t mcp2515_poll(mcp2515_t *dev, can_frame_t *frame);
/* Passive RX fast path: mode/TEC/REC checked at least every 10 ms when polled.
 * CANINTF/EFLG remain checked for every frame; active devices use full checks. */
esp_err_t mcp2515_poll_fast(mcp2515_t *dev, can_frame_t *frame);

esp_err_t mcp2515_init_active(mcp2515_t *dev, const mcp2515_io_t *io, unsigned osc_mhz);
/* Single outstanding standard data frame, no retransmission after arbitration
 * loss or bus error. Caller serializes SPI access. */
esp_err_t mcp2515_tx_begin(mcp2515_t *dev, const can_frame_t *frame);
/* NOT_FOUND=pending, OK=confirmed TX0IF, other=latched fault. */
esp_err_t mcp2515_tx_status(mcp2515_t *dev);
esp_err_t mcp2515_tx_abort(mcp2515_t *dev);

/* Only discovery may tolerate a completed one-shot failure (no ACK). Bus-off,
 * mode/SPI faults remain fatal; no controller reset or automatic retransmit. */
esp_err_t mcp2515_probe_cancel(mcp2515_t *dev);
