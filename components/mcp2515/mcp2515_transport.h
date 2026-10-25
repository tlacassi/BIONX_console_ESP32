/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "mcp2515_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    mcp2515_stats_t chip;
    uint32_t capture_dropped;
    unsigned queue_used;
    esp_err_t fault;
    int64_t last_rx_us;
    /* Component-test timing, cumulative since startup; zero in other profiles. */
    uint32_t rx_polls, max_poll_us, irq_edges;
    uint64_t rx_poll_us;
} mcp2515_transport_stats_t;

/* Startup only, creates the RX task and bounded static capture queue. */
esp_err_t mcp2515_transport_start(void);
bool mcp2515_capture_receive(can_frame_t *out, TickType_t wait);
esp_err_t mcp2515_transport_snapshot(mcp2515_transport_stats_t *out);

/* Only enabled in the explicit master firmware profile. No automatic retry. */
esp_err_t mcp2515_transport_start_master(void);
esp_err_t mcp2515_transport_send(const can_frame_t *frame, unsigned timeout_ms);

/* Restricted battery HW-version query; failed probes do not latch TX fault. */
esp_err_t mcp2515_transport_probe(const can_frame_t *frame, unsigned timeout_ms);
