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
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    printf("UI/demo tests passed; LVGL pool used=%u%%, largest free=%lu\n",
        mem.used_pct, (unsigned long)mem.free_biggest_size);
    lv_deinit();
    return 0;
}
