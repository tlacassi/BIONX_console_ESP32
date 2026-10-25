/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "esp_lcd_panel_io.h"
/* Write-only, 8-bit command/parameter GC9A01 IO, synchronous completion. */
esp_err_t shared_lcd_io_new(const esp_lcd_panel_io_spi_config_t *cfg,
                            esp_lcd_panel_io_handle_t *out);
