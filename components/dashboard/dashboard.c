/* SPDX-License-Identifier: GPL-3.0-only */
#include "sdkconfig.h"
#ifdef CONFIG_BIONX_DASHBOARD_DEMO
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include "dashboard.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "dashboard";
static lv_obj_t *speed, *ring, *mode_badge, *mode_label, *battery, *power;
static assist_mode_t current_mode;
static int32_t shown_speed;
static int32_t speed_from, speed_target;
static uint32_t speed_tick, mode_tick;
static char speed_text[16], battery_text[8], power_text[12];
static lv_opa_t mode_opacity = LV_OPA_COVER;
static bool initialized;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    return obj;
}

/* Called only by LVGL while its task holds the port lock. */
static void animate_speed(void *unused, int32_t value)
{
    (void)unused;
    shown_speed = value;
    snprintf(speed_text, sizeof(speed_text), "%ld.%ld", (long)(value / 10), (long)(value % 10));
    lv_label_set_text_static(speed, speed_text);
    lv_arc_set_value(ring, value);
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    uint32_t dt = lv_tick_elaps(speed_tick);
    if (dt > 100) dt = 100;
    int32_t value = speed_from + (speed_target - speed_from) * (int32_t)dt / 100;
    if (value != shown_speed) animate_speed(NULL, value);
    uint32_t fade = lv_tick_elaps(mode_tick);
    if (fade > 350) fade = 350;
    const lv_opa_t opacity = (lv_opa_t)(100 + 155 * fade / 350);
    if (opacity != mode_opacity) {
        lv_obj_set_style_opa(mode_badge, opacity, 0);
        mode_opacity = opacity;
    }
}

static void set_mode(assist_mode_t mode, bool animate)
{
    static const char *names[] = {"ECO", "TOUR", "SPORT"};
    static const uint32_t colors[] = {0x53F5A1, 0x41DCF5, 0xFFB65C};
    const lv_color_t accent = lv_color_hex(colors[mode]);
    lv_label_set_text(mode_label, names[mode]);
    lv_obj_center(mode_label);
    lv_obj_set_style_text_color(mode_label, accent, 0);
    lv_obj_set_style_border_color(mode_badge, accent, 0);
    lv_obj_set_style_arc_color(ring, accent, LV_PART_INDICATOR);
    if (animate) {
        mode_tick = lv_tick_get();
        lv_obj_set_style_opa(mode_badge, 100, 0);
        mode_opacity = 100;
    }
    current_mode = mode;
}

esp_err_t dashboard_init(void)
{
    if (initialized) return ESP_ERR_INVALID_STATE;
    if (!lvgl_port_lock(1000)) return ESP_ERR_TIMEOUT;
    lv_display_t *display = lv_display_get_default();
    if (!display) {
        lvgl_port_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080E12), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    ring = lv_arc_create(screen);
    lv_obj_set_size(ring, 224, 224);
    lv_obj_center(ring);
    lv_arc_set_bg_angles(ring, 135, 405);
    lv_arc_set_range(ring, 0, 450);
    lv_arc_set_value(ring, 0);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_arc_width(ring, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0x203038), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);

    mode_badge = lv_obj_create(screen);
    lv_obj_remove_style_all(mode_badge);
    lv_obj_set_size(mode_badge, 96, 26);
    lv_obj_align(mode_badge, LV_ALIGN_TOP_MID, 0, 39);
    lv_obj_set_style_bg_color(mode_badge, lv_color_hex(0x112127), 0);
    lv_obj_set_style_bg_opa(mode_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mode_badge, 13, 0);
    lv_obj_set_style_border_width(mode_badge, 1, 0);
    lv_obj_set_style_border_opa(mode_badge, LV_OPA_40, 0);
    lv_obj_remove_flag(mode_badge, LV_OBJ_FLAG_SCROLLABLE);
    mode_label = label(mode_badge, "ECO", 0, &lv_font_montserrat_16, 0x53F5A1);
    set_mode(ASSIST_MODE_ECO, false);

    speed = label(screen, "0.0", 82, &lv_font_montserrat_48, 0xF4FBFF);
    lv_obj_set_width(speed, 190);
    lv_obj_set_style_text_align(speed, LV_TEXT_ALIGN_CENTER, 0);
    label(screen, "km/h", 138, &lv_font_montserrat_12, 0x93A8B3);
    lv_obj_t *soc_title = label(screen, "BATTERY", 166, &lv_font_montserrat_12, 0x93A8B3);
    lv_obj_set_x(soc_title, -44);
    lv_obj_t *watts_title = label(screen, "MOTOR", 166, &lv_font_montserrat_12, 0x93A8B3);
    lv_obj_set_x(watts_title, 44);
    battery = label(screen, "--%", 183, &lv_font_montserrat_20, 0xF4FBFF);
    lv_obj_set_x(battery, -44);
    power = label(screen, "-- W", 183, &lv_font_montserrat_20, 0xF4FBFF);
    lv_obj_set_x(power, 44);
    label(screen, "DEMO", 214, &lv_font_montserrat_12, 0x93A8B3);

    if (!lv_timer_create(refresh, 33, NULL)) {
        ESP_LOGE(TAG, "Cannot allocate dashboard refresh timer");
        lvgl_port_unlock();
        return ESP_ERR_NO_MEM;
    }
    initialized = true;
    ESP_LOGI(TAG, "Dashboard ready (partial render, target 30 FPS); heap=%u, min=%u",
        (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
    lvgl_port_unlock();
    return ESP_OK;
}
esp_err_t dashboard_update(const dashboard_data_t *data)
{
    if (!data || !isfinite(data->speed_kmh) || data->speed_kmh < 0 ||
        data->speed_kmh > 45 || data->battery_soc > 100 ||
        (unsigned)data->assist_mode > ASSIST_MODE_SPORT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized) return ESP_ERR_INVALID_STATE;
    if (!lvgl_port_lock(20)) return ESP_ERR_TIMEOUT;
    const int32_t target = (int32_t)lroundf(data->speed_kmh * 10);
    if (target != shown_speed) {
        speed_from = shown_speed;
        speed_target = target;
        speed_tick = lv_tick_get();
    }
    if (data->assist_mode != current_mode) set_mode(data->assist_mode, true);
    /* No widget recreation; unchanged text is cached to avoid allocations. */
    static int last_soc = -1, last_power = -1;
    if (last_soc != data->battery_soc) {
        snprintf(battery_text, sizeof(battery_text), "%u%%", data->battery_soc);
        lv_label_set_text_static(battery, battery_text);
        last_soc = data->battery_soc;
    }
    if (last_power != data->motor_power_w) {
        snprintf(power_text, sizeof(power_text), "%u W", data->motor_power_w);
        lv_label_set_text_static(power, power_text);
        last_power = data->motor_power_w;
    }
    lvgl_port_unlock();
    return ESP_OK;
}
#endif
