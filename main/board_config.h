/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "sdkconfig.h"
#include "esp_err.h"
#define BOARD_CAN_TX CONFIG_BIONX_TX
#define BOARD_CAN_RX CONFIG_BIONX_RX
#define BOARD_I2C_SDA CONFIG_BIONX_I2C_SDA
#define BOARD_I2C_SCL CONFIG_BIONX_I2C_SCL
#define BOARD_BMS_TX CONFIG_BIONX_BMS_TX
#define BOARD_BMS_RX CONFIG_BIONX_BMS_RX
#define BOARD_ENCODER_A CONFIG_BIONX_ENCODER_A
#define BOARD_ENCODER_B CONFIG_BIONX_ENCODER_B
#define BOARD_ENCODER_BUTTON CONFIG_BIONX_ENCODER_BUTTON
/* These pins are reserved here; I2C/UART/encoder drivers are not started yet. */
esp_err_t board_config_validate(void);
