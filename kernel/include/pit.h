/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Programmable Interval Timer (PIT 8253/8254) Interface
 * Frequency calibration, tick accounting, and calibrated delays.
 * GPLv3 Licensed.
 */

#ifndef SELD_PIT_H
#define SELD_PIT_H

#include <stdint.h>
#include <stddef.h>

#define PIT_TARGET_HZ 100 // 100 Hz = 10 ms per tick

void pit_init(uint32_t frequency);
void pit_handle_interrupt(void);
uint64_t pit_get_ticks(void);
uint64_t pit_get_uptime_sec(void);
uint64_t pit_get_uptime_ms(void);
void pit_sleep_ms(uint64_t ms);

#endif /* SELD_PIT_H */
