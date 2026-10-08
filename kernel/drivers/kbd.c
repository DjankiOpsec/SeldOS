/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * PS/2 Keyboard Driver with Ring Buffer Implementation.
 * GPLv3 Licensed.
 */

#include "kbd.h"
#include "rand.h"

#define KBD_BUFFER_SIZE 256
#define KBD_EVENT_QUEUE_SIZE 128
#define KBD_STAGE_SIZE 256

static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile size_t buf_head = 0;
static volatile size_t buf_tail = 0;

static struct kbd_event kbd_event_queue[KBD_EVENT_QUEUE_SIZE];
static volatile size_t ev_head = 0;
static volatile size_t ev_tail = 0;

/* THL Keystroke Timing Obfuscation & Jitter Queues */
static char kbd_stage_buf[KBD_STAGE_SIZE];
static volatile size_t stage_head = 0;
static volatile size_t stage_tail = 0;

static struct kbd_event kbd_stage_events[KBD_STAGE_SIZE];
static volatile size_t stage_ev_head = 0;
static volatile size_t stage_ev_tail = 0;

static volatile int s_kbd_jitter_enabled = 0;
static volatile uint32_t s_jitter_ticks = 0;
static volatile uint32_t s_jitter_target = 5; /* 50ms at 100 Hz PIT */

static int shift_pressed = 0;
static int caps_lock = 0;

static const char scancode_ascii_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char scancode_ascii_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void kbd_flush_staged(void) {
    /* 1. Flush raw events */
    while (stage_ev_tail != stage_ev_head) {
        size_t next_ev = (ev_head + 1) % KBD_EVENT_QUEUE_SIZE;
        if (next_ev != ev_tail) {
            kbd_event_queue[ev_head] = kbd_stage_events[stage_ev_tail];
            ev_head = next_ev;
        }
        stage_ev_tail = (stage_ev_tail + 1) % KBD_STAGE_SIZE;
    }

    /* 2. Flush translated ASCII buffer */
    while (stage_tail != stage_head) {
        size_t next = (buf_head + 1) % KBD_BUFFER_SIZE;
        if (next != buf_tail) {
            kbd_buffer[buf_head] = kbd_stage_buf[stage_tail];
            buf_head = next;
        }
        stage_tail = (stage_tail + 1) % KBD_STAGE_SIZE;
    }
}

void kbd_init(void) {
    buf_head = 0;
    buf_tail = 0;
    ev_head = 0;
    ev_tail = 0;
    stage_head = 0;
    stage_tail = 0;
    stage_ev_head = 0;
    stage_ev_tail = 0;
    s_kbd_jitter_enabled = 0;
    s_jitter_ticks = 0;
    s_jitter_target = 5;
    shift_pressed = 0;
    caps_lock = 0;
}

void kbd_timer_tick(void) {
    if (!s_kbd_jitter_enabled) {
        if (stage_head != stage_tail || stage_ev_head != stage_ev_tail) {
            kbd_flush_staged();
        }
        return;
    }

    s_jitter_ticks++;
    if (s_jitter_ticks >= s_jitter_target) {
        s_jitter_ticks = 0;
        /* Dynamic micro-jitter: 4..6 ticks (40..60ms) using CSPRNG */
        s_jitter_target = 4 + (uint32_t)(rng_get_u64() % 3);
        kbd_flush_staged();
    }
}

void kbd_set_jitter(int enable) {
    s_kbd_jitter_enabled = enable ? 1 : 0;
    s_jitter_ticks = 0;
    if (!s_kbd_jitter_enabled) {
        kbd_flush_staged();
    }
}

int kbd_get_jitter(void) {
    return s_kbd_jitter_enabled;
}

void kbd_handle_scancode(uint8_t scancode) {
    if (scancode == 0xE0) {
        return;
    }

    // 1. Enqueue raw event for game input / polling
    uint8_t pressed = (scancode & 0x80) ? 0 : 1;
    uint8_t code = scancode & 0x7F;

    if (s_kbd_jitter_enabled) {
        size_t next_ev = (stage_ev_head + 1) % KBD_STAGE_SIZE;
        if (next_ev != stage_ev_tail) {
            kbd_stage_events[stage_ev_head].scancode = code;
            kbd_stage_events[stage_ev_head].pressed = pressed;
            stage_ev_head = next_ev;
        }
    } else {
        size_t next_ev = (ev_head + 1) % KBD_EVENT_QUEUE_SIZE;
        if (next_ev != ev_tail) {
            kbd_event_queue[ev_head].scancode = code;
            kbd_event_queue[ev_head].pressed = pressed;
            ev_head = next_ev;
        }
    }

    // 2. ASCII translation for console/shell
    if (scancode == 0x2A || scancode == 0x36) { // Left/Right shift press
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) { // Left/Right shift release
        shift_pressed = 0;
        return;
    }
    if (scancode == 0x3A) { // Caps lock press
        caps_lock = !caps_lock;
        return;
    }

    if (scancode & 0x80) {
        return; // Key release
    }

    char c = 0;
    int is_upper = shift_pressed ^ caps_lock;

    if (is_upper && scancode < 128) {
        c = scancode_ascii_upper[scancode];
    } else if (scancode < 128) {
        c = scancode_ascii_lower[scancode];
    }

    if (c != 0) {
        if (s_kbd_jitter_enabled) {
            size_t next = (stage_head + 1) % KBD_STAGE_SIZE;
            if (next != stage_tail) {
                kbd_stage_buf[stage_head] = c;
                stage_head = next;
            }
        } else {
            size_t next = (buf_head + 1) % KBD_BUFFER_SIZE;
            if (next != buf_tail) {
                kbd_buffer[buf_head] = c;
                buf_head = next;
            }
        }
    }
}

#include "serial.h"

int kbd_has_char(void) {
    if (serial_has_char()) return 1;
    return buf_head != buf_tail;
}

char kbd_getchar(void) {
    while (!kbd_has_char()) {
#if defined(__riscv)
        __asm__ volatile ("nop");
#else
        __asm__ volatile ("pause");
#endif
    }
    if (serial_has_char()) {
        return serial_getchar();
    }
    char c = kbd_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

static uint8_t serial_char_to_scancode(char c) {
    if (c == '\r' || c == '\n') return 0x1C;
    if (c == '\b' || c == 0x7F) return 0x0E;
    if (c == '\t') return 0x0F;
    if (c == 27)   return 0x01;
    if (c == ' ')  return 0x39;

    for (uint8_t i = 0; i < 128; i++) {
        if (scancode_ascii_lower[i] == c) return i;
    }
    if (c >= 'A' && c <= 'Z') {
        char lower = c - 'A' + 'a';
        for (uint8_t i = 0; i < 128; i++) {
            if (scancode_ascii_lower[i] == lower) return i;
        }
    }
    for (uint8_t i = 0; i < 128; i++) {
        if (scancode_ascii_upper[i] == c) return i;
    }
    return 0;
}

int kbd_poll_event(struct kbd_event* ev) {
    if (!ev) return 0;
    if (ev_head != ev_tail) {
        *ev = kbd_event_queue[ev_tail];
        ev_tail = (ev_tail + 1) % KBD_EVENT_QUEUE_SIZE;
        return 1;
    }
    if (serial_has_char()) {
        char c = serial_getchar();
        if (c == 27 && serial_has_char()) {
            char c2 = serial_getchar();
            if (c2 == '[' && serial_has_char()) {
                char c3 = serial_getchar();
                if (c3 == 'A') { ev->scancode = 0x48; ev->pressed = 1; return 1; } // Up
                if (c3 == 'B') { ev->scancode = 0x50; ev->pressed = 1; return 1; } // Down
                if (c3 == 'C') { ev->scancode = 0x4D; ev->pressed = 1; return 1; } // Right
                if (c3 == 'D') { ev->scancode = 0x4B; ev->pressed = 1; return 1; } // Left
            }
        }
        uint8_t sc = serial_char_to_scancode(c);
        if (sc != 0) {
            ev->scancode = sc;
            ev->pressed = 1;
            return 1;
        }
    }
    return 0;
}

