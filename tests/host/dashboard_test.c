/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "dashboard_demo.h"
#include "lvgl.h"

bool test_lock_available = true;
static uint16_t framebuffer[240 * 240], draw_buffer[240 * 20];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    size_t width = (size_t)lv_area_get_width(area);
    for (int y = area->y1; y <= area->y2; ++y) {
        memcpy(&framebuffer[y * 240 + area->x1], pixels, width * 2);
        pixels += width * 2;
    }
    lv_display_flush_ready(display);
}

static void check_labels(lv_obj_t *parent)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *obj = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(obj, &lv_label_class)) {
            lv_area_t a;
            lv_obj_get_coords(obj, &a);
            /* Only the speed label has a deliberately wide fixed text box;
             * check the actual centered glyph extents instead. */
            lv_point_t size;
            lv_text_get_size(&size, lv_label_get_text(obj),
                lv_obj_get_style_text_font(obj, 0), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            int x1 = (a.x1 + a.x2 + 1 - size.x) / 2;
            int x2 = x1 + size.x - 1;
            int y2 = a.y1 + size.y - 1;
            int xs[] = {x1, x2}, ys[] = {a.y1, y2};
            for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y) {
                int dx = xs[x] - 120, dy = ys[y] - 120;
                if (dx * dx + dy * dy >= 120 * 120) {
                    fprintf(stderr, "Clipped label: %s (%d,%d)\n", lv_label_get_text(obj), xs[x], ys[y]);
                    assert(false);
                }
            }
        }
        check_labels(obj);
    }
}

int main(void)
{
    dashboard_data_t data;
    dashboard_demo_sample(0, &data);
    assert(data.speed_kmh == 0 && data.battery_soc == 78 && data.motor_power_w == 0);
    assert(data.assist_mode == ASSIST_MODE_ECO);
    dashboard_demo_sample(12000000, &data);
    assert(fabsf(data.speed_kmh - 35) < 0.001f && data.motor_power_w == 500);
    dashboard_demo_sample(120000000, &data);
    assert(data.battery_soc == 77);
    dashboard_demo_sample(UINT64_MAX, &data);
    assert(data.battery_soc == 0 && data.speed_kmh >= 0 && data.speed_kmh <= 35);

    lv_init();
    assert(dashboard_init() == ESP_ERR_INVALID_STATE);
    lv_display_t *display = lv_display_create(240, 240);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    assert(dashboard_init() == ESP_OK);
    assert(dashboard_init() == ESP_ERR_INVALID_STATE);
    assert(dashboard_update(NULL) == ESP_ERR_INVALID_ARG);
    data.speed_kmh = NAN;
    assert(dashboard_update(&data) == ESP_ERR_INVALID_ARG);
    dashboard_demo_sample(0, &data);
    data.battery_soc = 101;
    assert(dashboard_update(&data) == ESP_ERR_INVALID_ARG);
    data.battery_soc = 78;
    data.assist_mode = (assist_mode_t)-1;
    assert(dashboard_update(&data) == ESP_ERR_INVALID_ARG);
    dashboard_demo_sample(0, &data);
    test_lock_available = false;
    assert(dashboard_update(&data) == ESP_ERR_TIMEOUT);
    test_lock_available = true;
    for (uint64_t t = 0; t <= 24000000; t += 100000) {
        dashboard_demo_sample(t, &data);
        assert(data.speed_kmh >= 0 && data.speed_kmh <= 35);
        assert(data.motor_power_w <= 500);
        assert(dashboard_update(&data) == ESP_OK);
        for (int step = 0; step < 10; ++step) {
            lv_tick_inc(10);
            lv_timer_handler();
        }
        lv_obj_update_layout(lv_screen_active());
        check_labels(lv_screen_active());
        if (t == 12000000) {
            FILE *file = fopen("dashboard.rgb565", "wb");
            assert(file);
            assert(fwrite(framebuffer, sizeof(framebuffer), 1, file) == 1);
            fclose(file);
        }
    }
    dashboard_live_t real={.speed_valid=true,.speed_kmh=18.6f,.soc_valid=true,
        .battery_soc=74,.power_valid=true,.battery_power_w=123,.command_valid=true,
        .command=4,.motor_level_valid=true,.motor_level=32,.torque_valid=true,.torque=20,.status="ASSIST X2"};
    assert(dashboard_live_update(NULL)==ESP_ERR_INVALID_ARG);
    real.battery_power_w=NAN;assert(dashboard_live_update(&real)==ESP_ERR_INVALID_ARG);
    real.battery_power_w=123;test_lock_available=false;
    assert(dashboard_live_update(&real)==ESP_ERR_TIMEOUT);test_lock_available=true;
    assert(dashboard_live_update(&real)==ESP_OK);
    lv_tick_inc(50);lv_timer_handler();lv_obj_update_layout(lv_screen_active());
    check_labels(lv_screen_active());
    lv_obj_t *motor_arc=NULL,*pedal_bar=NULL;
    for(uint32_t k=0;k<lv_obj_get_child_count(lv_screen_active());k++) {
        lv_obj_t *obj=lv_obj_get_child(lv_screen_active(),k);
        if(lv_obj_check_type(obj,&lv_arc_class)) motor_arc=obj;
        if(lv_obj_check_type(obj,&lv_bar_class)) pedal_bar=obj;
    }
    assert(motor_arc && pedal_bar);
    assert(lv_arc_get_value(motor_arc)==32 && lv_bar_get_value(pedal_bar)==20);
    real.speed_kmh=0;assert(dashboard_live_update(&real)==ESP_OK);
    assert(lv_arc_get_value(motor_arc)==32); /* ring does not follow speed */
    real.speed_kmh=18.6f;
    FILE *live_file=fopen("dashboard-live.rgb565","wb");assert(live_file);
    assert(fwrite(framebuffer,sizeof(framebuffer),1,live_file)==1);fclose(live_file);
    for(int watts=-1400;watts<=1400;watts+=1400) {
        real.battery_power_w=watts;real.status="DATA OLD/LINK";
        assert(dashboard_live_update(&real)==ESP_OK);
        lv_tick_inc(50);lv_timer_handler();lv_obj_update_layout(lv_screen_active());
        check_labels(lv_screen_active());
    }
    const char *states[]={"WAIT ZERO","ZERO PEDALS","WAIT NEUTRAL","ASSIST X2",
        "DRY RUN TX OFF","DATA OLD/LINK","PEDALS LOADED","CAN INIT FAIL"};
    for(unsigned k=0;k<sizeof(states)/sizeof(states[0]);k++) {
        real.status=states[k];assert(dashboard_live_update(&real)==ESP_OK);
        lv_tick_inc(50);lv_timer_handler();lv_obj_update_layout(lv_screen_active());
        check_labels(lv_screen_active());
    }
    real.fault=true;real.status="CAN INIT FAIL";
    assert(dashboard_live_update(&real)==ESP_OK);
    lv_tick_inc(600);lv_timer_handler();
    bool fault_seen=false;
    for(uint32_t k=0;k<lv_obj_get_child_count(lv_screen_active());k++) {
        lv_obj_t *obj=lv_obj_get_child(lv_screen_active(),k);
        if(lv_obj_check_type(obj,&lv_label_class) && !strcmp(lv_label_get_text(obj),"CAN INIT FAIL")) fault_seen=true;
    }
    assert(fault_seen);real.fault=false;
    real.fault=false;real.power_valid=true;real.battery_power_w=0;
    assert(dashboard_live_update(&real)==ESP_OK);
    bool zero_watts_seen=false;
    for(uint32_t k=0;k<lv_obj_get_child_count(lv_screen_active());k++) {
        lv_obj_t *obj=lv_obj_get_child(lv_screen_active(),k);
        if(lv_obj_check_type(obj,&lv_label_class) && !strcmp(lv_label_get_text(obj),"+0 W")) zero_watts_seen=true;
    }
    assert(zero_watts_seen); /* a valid idle measurement must not disappear */
    real.power_valid=false;real.speed_valid=false;real.soc_valid=false;
    real.motor_level_valid=false;real.torque_valid=false;
    real.command_valid=false;real.status="WAIT BATTERY";
    assert(dashboard_live_update(&real)==ESP_OK);
    lv_tick_inc(50);lv_timer_handler();lv_obj_update_layout(lv_screen_active());
    check_labels(lv_screen_active());
    assert(!lv_arc_get_value(motor_arc) && !lv_bar_get_value(pedal_bar));
    lv_tick_inc(600);lv_timer_handler();lv_obj_update_layout(lv_screen_active());
    bool old_seen=false;
    for(uint32_t k=0;k<lv_obj_get_child_count(lv_screen_active());k++) {
        lv_obj_t *obj=lv_obj_get_child(lv_screen_active(),k);
        if(lv_obj_check_type(obj,&lv_label_class) && !strcmp(lv_label_get_text(obj),"DATA OLD")) old_seen=true;
    }
    assert(old_seen);
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    printf("UI/demo tests passed; LVGL pool used=%u%%, largest free=%lu\n",
        mem.used_pct, (unsigned long)mem.free_biggest_size);
    lv_deinit();
    return 0;
}
