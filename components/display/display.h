/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "esp_err.h"

/* Boot-time only, before dashboard_init(). Configured by the demo profile. */
esp_err_t display_init(void);
