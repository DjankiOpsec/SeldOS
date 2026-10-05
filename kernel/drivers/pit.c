/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Programmable Interval Timer (PIT 8253/8254) Implementation
 * Base frequency: 1.193182 MHz
 * Mode 3 (Square Wave Generator) on Channel 0
 * GPLv3 Licensed.
 */

#include "pit.h"
#include "audio.h"
#include "io.h"
#include "serial.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND_REG   0x43
#define PIT_BASE_FREQ     1193182

static volatile uint64_t timer_ticks = 0;
static uint32_t current_hz = PIT_TARGET_HZ;

void pit_init(uint32_t frequency) {
    if (frequency == 0) frequency = PIT_TARGET_HZ;
    current_hz = frequency;

    uint32_t divisor = PIT_BASE_FREQ / frequency;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;

    // Command: Channel 0, Access mode lobyte/hibyte, Mode 3 (square wave), 16-bit binary
    // 00 (ch0) | 11 (lo/hi) | 011 (mode 3) | 0 (binary) = 0x36
    outb(PIT_COMMAND_REG, 0x36);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    timer_ticks = 0;
    serial_puts("[+] PIT: Calibrated to ");
    serial_print_dec(frequency);
    serial_puts(" Hz (Divisor: ");
    serial_print_dec(divisor);
    serial_puts(")\n");
}

void pit_handle_interrupt(void) {
    timer_ticks++;
    audio_timer_tick();
}

uint64_t pit_get_ticks(void) {
    return timer_ticks;
}

uint64_t pit_get_uptime_ms(void) {
    return (timer_ticks * 1000) / current_hz;
}

uint64_t pit_get_uptime_sec(void) {
    return timer_ticks / current_hz;
}

void pit_sleep_ms(uint64_t ms) {
    if (ms == 0) return;
    // Bound sleep duration to 60 seconds max to prevent tick overflows & infinite freezes
    if (ms > 60000) ms = 60000;
    if (current_hz == 0) current_hz = PIT_TARGET_HZ;

    uint64_t delta_ticks = (ms * current_hz + 999) / 1000;
    if (delta_ticks == 0) delta_ticks = 1;

    uint64_t start_ticks = timer_ticks;
    while ((timer_ticks - start_ticks) < delta_ticks) {
        __asm__ volatile ("hlt");
    }
}
