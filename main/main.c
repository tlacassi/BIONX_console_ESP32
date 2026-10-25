/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_config.h"
#include "bionx_diagnostic.h"
#include "can_observer.h"
#include "bionx_master_app.h"
#include "component_test.h"
#ifdef CONFIG_BIONX_DASHBOARD_DEMO
#include "display.h"
#include "dashboard.h"
#include "dashboard_demo.h"
#include "esp_timer.h"
#include "esp_system.h"
#endif

void app_main(void)
{
#ifdef CONFIG_BIONX_CAN_MASTER
    bionx_master_app_run();
#elif defined(CONFIG_BIONX_COMPONENT_TEST)
    component_test_run();
#elif defined(CONFIG_BIONX_CAN_OBSERVE)
    can_observer_run();
#elif defined(CONFIG_BIONX_SCREEN_TEST)
    screen_test_run();
#elif defined(CONFIG_BIONX_DASHBOARD_DEMO)
    ESP_LOGI("app", "Dashboard DEMO: simulated values; CAN disabled");
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(dashboard_init());
    const int64_t start = esp_timer_get_time();
    TickType_t last = xTaskGetTickCount();
    unsigned dropped = 0;
    unsigned samples = 0;
    for (;;) {
        dashboard_data_t sample;
        dashboard_demo_sample((uint64_t)(esp_timer_get_time() - start), &sample);
        if (dashboard_update(&sample) == ESP_ERR_TIMEOUT && ++dropped % 50 == 1) {
            ESP_LOGW("app", "UI busy: skipped updates=%u", dropped);
        }
        if (++samples % 100 == 0) {
            ESP_LOGI("app", "DEMO alive; heap=%u min=%u; main stack margin=%u bytes; skipped=%u",
                (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
                (unsigned)uxTaskGetStackHighWaterMark(NULL), dropped);
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    }
#elif defined(CONFIG_BIONX_USB_TEST_ONLY)
    ESP_LOGI("app", "BionX / ESP32-C3 / USB boot test only");
    ESP_LOGI("app", "CAN and display drivers disabled; screen GPIOs untouched");
    unsigned uptime = 0;
    for (;;) {
        ESP_LOGI("app", "USB test alive: %u s", uptime);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ++uptime;
    }
#else
    ESP_ERROR_CHECK(board_config_validate());
    ESP_LOGI("app", "BionX / ESP32-C3 Super Mini / USB console");
    ESP_LOGI("app", "CAN TX=%d RX=%d; no motor actuation", BOARD_CAN_TX, BOARD_CAN_RX);
    if (xTaskCreate(bionx_diagnostic_task, "bionx_diag", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE("app", "Cannot create CAN diagnostic task");
        abort();
    }
#endif
}
