/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { BX_BUTTON_NONE, BX_BUTTON_START, BX_BUTTON_STOP } bx_button_event_t;
typedef struct {
    bool raw, stable, armed, held;
    int64_t changed_us, pressed_us;
} bx_button_t;
/* Active-low GPIO converted to pressed=true by caller. Initial release required. */
static inline bx_button_event_t bx_button_tick(bx_button_t *b, bool pressed, int64_t now)
{
    if (pressed!=b->raw) { b->raw=pressed; b->changed_us=now; }
    if (now-b->changed_us<25000) return BX_BUTTON_NONE;
    if (!b->armed) {
        if (!pressed) { b->armed=true; b->stable=false; }
        return BX_BUTTON_NONE;
    }
    if (pressed!=b->stable) {
        b->stable=pressed;
        if (pressed) { b->pressed_us=now; b->held=false; }
        else {
            bool stop=!b->held && now-b->pressed_us>=2000000;
            if (stop) { b->held=true; return BX_BUTTON_STOP; }
            if (!b->held) return BX_BUTTON_START;
        }
    }
    if (b->stable && !b->held && now-b->pressed_us>=2000000) {
        b->held=true; return BX_BUTTON_STOP;
    }
    return BX_BUTTON_NONE;
}
