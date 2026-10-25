#pragma once
#include <stdbool.h>
#include <stdint.h>
extern bool test_lock_available;
static inline bool lvgl_port_lock(uint32_t timeout) { (void)timeout; return test_lock_available; }
static inline void lvgl_port_unlock(void) {}
