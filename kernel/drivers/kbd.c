/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * PS/2 Keyboard Driver with Ring Buffer Implementation.
 * GPLv3 Licensed.
 */

#include "kbd.h"

#define KBD_BUFFER_SIZE 256
#define KBD_EVENT_QUEUE_SIZE 128

static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile size_t buf_head = 0;
static volatile size_t buf_tail = 0;

static struct kbd_event kbd_event_queue[KBD_EVENT_QUEUE_SIZE];
static volatile size_t ev_head = 0;
static volatile size_t ev_tail = 0;

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

void kbd_init(void) {
    buf_head = 0;
    buf_tail = 0;
    ev_head = 0;
    ev_tail = 0;
    shift_pressed = 0;
    caps_lock = 0;
}

void kbd_handle_scancode(uint8_t scancode) {
    if (scancode == 0xE0) {
        return;
    }

    // 1. Enqueue raw event for game input / polling
    uint8_t pressed = (scancode & 0x80) ? 0 : 1;
    uint8_t code = scancode & 0x7F;

    size_t next_ev = (ev_head + 1) % KBD_EVENT_QUEUE_SIZE;
    if (next_ev != ev_tail) {
        kbd_event_queue[ev_head].scancode = code;
        kbd_event_queue[ev_head].pressed = pressed;
        ev_head = next_ev;
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
        size_t next = (buf_head + 1) % KBD_BUFFER_SIZE;
        if (next != buf_tail) {
            kbd_buffer[buf_head] = c;
            buf_head = next;
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
        __asm__ volatile ("pause");
    }
    if (serial_has_char()) {
        return serial_getchar();
    }
    char c = kbd_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

int kbd_poll_event(struct kbd_event* ev) {
    if (ev_head == ev_tail || !ev) {
        return 0;
    }
    *ev = kbd_event_queue[ev_tail];
    ev_tail = (ev_tail + 1) % KBD_EVENT_QUEUE_SIZE;
    return 1;
}
