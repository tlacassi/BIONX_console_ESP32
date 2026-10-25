/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "bionx_master.h"
#include "mcp2515_transport.h"
bool bench_app_init(void);
void bench_app_tick(bx_master_t *m,const mcp2515_transport_stats_t *stats,int64_t now);
bool bench_app_command(const char *command,int64_t now);
bool bench_app_tx_allowed(const can_frame_t *f,int64_t now);
void bench_app_status(char *text,size_t size);
