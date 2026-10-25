/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#include "component_test.h"
#ifdef CONFIG_BIONX_SCREEN_TEST
#include "driver/gpio.h"
#include "display.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
void screen_test_run(void)
{
    /* Shared SPI module remains electrically deselected; no MCP init or CAN TX. */
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_33,1));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_33,GPIO_MODE_OUTPUT));
    ESP_LOGI("screen_test","Screen only: SDA/MO=18 SCL/SCK=5 CS=14 DC=32 RST=27; SPI=%dMHz",CONFIG_BIONX_LCD_CLOCK_MHZ);
    esp_err_t err=display_init();
    if (err!=ESP_OK) { ESP_LOGE("screen_test","init: %s",esp_err_to_name(err)); return; }
    lv_obj_t *label=NULL;
    if (lvgl_port_lock(1000)) {
        label=lv_label_create(lv_screen_active());
        lv_obj_set_style_text_font(label,&lv_font_montserrat_20,0);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_center(label);
        lvgl_port_unlock();
    }
    const uint32_t colors[]={0xff0000,0x00ff00,0x0000ff,0xffffff};
    const char *names[]={"RED","GREEN","BLUE","WHITE"};
    for(unsigned i=0;;i=(i+1)%4) {
        if (label && lvgl_port_lock(1000)) {
            lv_obj_t *screen=lv_screen_active();
            lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
            lv_obj_set_style_bg_color(screen,lv_color_hex(colors[i]),0);
            lv_obj_set_style_text_color(label,lv_color_hex(i==3?0:0xffffff),0);
            lv_label_set_text_fmt(label,"GC9A01\n%s\nSPI %d MHz",names[i],CONFIG_BIONX_LCD_CLOCK_MHZ);
            lv_obj_center(label);
            lvgl_port_unlock();
        }
        ESP_LOGI("screen_test","Pattern %s",names[i]);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#else
void screen_test_run(void) {}
#endif
