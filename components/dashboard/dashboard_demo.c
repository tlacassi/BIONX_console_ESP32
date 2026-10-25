/* SPDX-License-Identifier: GPL-3.0-only */
#include <math.h>
#include <stddef.h>
#include "dashboard_demo.h"

void dashboard_demo_sample(uint64_t elapsed_us, dashboard_data_t *out)
{
    if (!out) return;
    /* 24 second stop/start cycle, starting at rest. Reduce modulo in integer
     * time so float precision does not degrade over long runtimes. */
    const float phase = (float)(elapsed_us % 24000000ULL) / 24000000.0f;
    const float wave = 0.5f - 0.5f * cosf(phase * 6.28318530718f);
    const uint64_t percent_used = elapsed_us / 120000000ULL;
    *out = (dashboard_data_t){
        .speed_kmh = 35.0f * wave,
        .battery_soc = percent_used >= 78 ? 0 : (uint8_t)(78 - percent_used),
        .motor_power_w = (uint16_t)lroundf(500.0f * wave * wave),
        .assist_mode = (assist_mode_t)((elapsed_us / 8000000ULL) % 3),
    };
}
