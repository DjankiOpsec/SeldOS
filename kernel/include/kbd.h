/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SELD_KBD_H
#define SELD_KBD_H

#include <stdint.h>
#include <stddef.h>

struct kbd_event {
    uint8_t scancode;
    uint8_t pressed;
};

void kbd_init(void);
void kbd_handle_scancode(uint8_t scancode);
int kbd_has_char(void);
char kbd_getchar(void);
int kbd_poll_event(struct kbd_event* ev);

/* THL Keystroke Timing Obfuscation & Jitter Interface */
void kbd_timer_tick(void);
void kbd_set_jitter(int enable);
int  kbd_get_jitter(void);

/* OpSec Dead Man's Grip (Shift Latch) Interface */
void kbd_set_deadman(int enable);
int  kbd_get_deadman(void);
int  kbd_is_deadman_armed(void);

/* OpSec Peripheral Blackout: Keyboard LED Extinguisher */
void kbd_set_leds(uint8_t led_mask);
void kbd_blackout_leds(void);

#endif /* SELD_KBD_H */
