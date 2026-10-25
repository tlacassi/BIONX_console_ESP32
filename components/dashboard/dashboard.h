/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
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

/* Real telemetry; validity is independent per field. Power is signed battery
 * electrical power, never inferred from the drive command. */
typedef struct {
    float speed_kmh, battery_power_w;
    uint8_t battery_soc, command;
    bool speed_valid, soc_valid, power_valid, command_valid, fault;
    const char *status;
} dashboard_live_t;
esp_err_t dashboard_live_update(const dashboard_live_t *data);
