/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "dashboard.h"

/* Pure time-based demo: elapsed monotonic microseconds since demo start.
 * No LVGL calls, task, allocation or dependency on UI state. */
void dashboard_demo_sample(uint64_t elapsed_us, dashboard_data_t *out);
