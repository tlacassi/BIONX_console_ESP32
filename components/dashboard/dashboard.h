/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    ASSIST_MODE_ECO,
    ASSIST_MODE_TOUR,
    ASSIST_MODE_SPORT,
} assist_mode_t;

typedef struct {
    float speed_kmh;
    uint8_t battery_soc;
    uint16_t motor_power_w;
    assist_mode_t assist_mode;
} dashboard_data_t;

/* Requires initialized display. Both APIs acquire the LVGL port lock.
 * update accepts complete, valid snapshots; timeout drops this update.
 * These values are presentation data, never motor commands. */
esp_err_t dashboard_init(void);
esp_err_t dashboard_update(const dashboard_data_t *data);
