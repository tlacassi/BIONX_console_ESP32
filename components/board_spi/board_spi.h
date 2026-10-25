/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/spi_master.h"

#define BOARD_SPI_HOST SPI2_HOST
#ifdef CONFIG_BIONX_MCP_DEDICATED_SPI
#define BOARD_MCP_SPI_HOST SPI3_HOST
#else
#define BOARD_MCP_SPI_HOST BOARD_SPI_HOST
#endif
/* One LCD line on a shared bus; ten lines when CAN has its own controller. */
#if (defined(CONFIG_BIONX_CAN_OBSERVE) || defined(CONFIG_BIONX_CAN_MASTER)) && !defined(CONFIG_BIONX_MCP_DEDICATED_SPI)
#define BOARD_LCD_LINES 1
#else
#define BOARD_LCD_LINES 10
#endif
/* Boot-time only. Bus is shared and retained for application lifetime. */
esp_err_t board_spi_init(void);

/* Task-only serialization; callers must initialize the bus first. */
void board_spi_lock(void);
void board_spi_unlock(void);

/* Independent in dedicated mode; otherwise the same recursive bus lock. */
void board_mcp_spi_lock(void);
void board_mcp_spi_unlock(void);
