/* SPDX-License-Identifier: GPL-3.0-only */
#include <stddef.h>
#include <stdbool.h>
#include "esp_log.h"
#include "board_config.h"

static bool allowed_pin(int pin)
{
    /* Header GPIOs, excluding USB, flash and boot straps 2/8/9. */
    return pin == 0 || pin == 1 || pin == 3 || pin == 4 || pin == 5 ||
           pin == 6 || pin == 7 || pin == 10 || pin == 20 || pin == 21;
}
esp_err_t board_config_validate(void)
{
    const int pins[] = {BOARD_CAN_TX, BOARD_CAN_RX, BOARD_I2C_SDA, BOARD_I2C_SCL,
        BOARD_BMS_TX, BOARD_BMS_RX, BOARD_ENCODER_A, BOARD_ENCODER_B, BOARD_ENCODER_BUTTON};
    for (size_t i = 0; i < sizeof(pins)/sizeof(pins[0]); ++i) {
        if (!allowed_pin(pins[i])) {
            ESP_LOGE("board", "GPIO %d is outside the supported Super Mini pin plan", pins[i]);
            return ESP_ERR_INVALID_ARG;
        }
        for (size_t j = 0; j < i; ++j) {
            if (pins[i] == pins[j]) {
                ESP_LOGE("board", "GPIO %d assigned twice", pins[i]);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    return ESP_OK;
}
