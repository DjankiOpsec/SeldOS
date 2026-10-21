/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * PS/2 Keyboard Driver with Ring Buffer Implementation.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "kbd.h"
#include "rand.h"
#include "io.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"

#define KBD_BUFFER_SIZE 256
#define KBD_EVENT_QUEUE_SIZE 128
#define KBD_STAGE_SIZE 256

/* OpSec Dead Man's Grip (Shift Latch) Interlock State */
static volatile int s_deadman_enabled = 1; /* Enabled by default as Sovereign Defense */
static volatile int s_deadman_armed = 0;   /* Armed once Shift is first latched */

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

static int left_shift_held = 0;
static int right_shift_pressed = 0;
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
    left_shift_held = 0;
    right_shift_pressed = 0;
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

static void opsec_deadman_trigger_wipe(void) {
    serial_puts("\n[!] ========================================================\n");
    serial_puts("[!] OPSEC EMERGENCY: DEAD MAN'S GRIP BROKEN (SHIFT RELEASED)!\n");
    serial_puts("[!] Physical contact severed. Hostile seizure detected!\n");
    serial_puts("[!] Executing Instant DoD RAM Scrubbing & CPU Hardware Reset...\n");
    serial_puts("[!] ========================================================\n");

    // 1. DoD wipe of physical memory
    pmm_secure_wipe_all_free();

#if defined(__riscv)
    *(volatile uint32_t*)(phys_to_virt(0x100000)) = 0x7777;
    while (1) {
        __asm__ volatile ("wfi");
    }
#else
    // 2. Write-back and invalidate all CPU L1/L2/L3 caches
    __asm__ volatile ("wbinvd" ::: "memory");

    // 3. Fast reset port 0x92
    outb(0x92, (uint8_t)(inb(0x92) | 1));

    // 4. ACPI/PCI hard reset port 0xCF9
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    outb(0xCF9, 0x0E);

    // 5. 8042 keyboard controller reset pulse
    for (int t = 0; t < 1000; t++) {
        uint8_t temp = inb(0x64);
        if (temp & 1) inb(0x60);
        if (!(temp & 2)) break;
    }
    outb(0x64, 0xFE);

    // 6. Halt execution permanently
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
#endif
}

void kbd_set_deadman(int enable) {
    s_deadman_enabled = enable ? 1 : 0;
    if (!s_deadman_enabled) {
        s_deadman_armed = 0;
    } else if (left_shift_held) {
        s_deadman_armed = 1;
    }
}

int kbd_get_deadman(void) {
    return s_deadman_enabled;
}

int kbd_is_deadman_armed(void) {
    return s_deadman_armed;
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
    if (scancode == 0x2A) { // Left Shift press -> Dead Man's Grip ONLY (no caps, no symbol shift)
        left_shift_held = 1;
        if (s_deadman_enabled) {
            s_deadman_armed = 1;
        }
        return;
    }
    if (scancode == 0xAA) { // Left Shift release -> Dead Man's Grip trigger
        left_shift_held = 0;
        if (s_deadman_enabled && s_deadman_armed) {
            opsec_deadman_trigger_wipe();
        }
        return;
    }
    if (scancode == 0x36) { // Right Shift press -> Typing modifier ONLY (Caps & symbols)
        right_shift_pressed = 1;
        return;
    }
    if (scancode == 0xB6) { // Right Shift release
        right_shift_pressed = 0;
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
    if (scancode < 128) {
        char base = scancode_ascii_lower[scancode];
        int is_alpha = (base >= 'a' && base <= 'z');
        int use_upper = is_alpha ? (right_shift_pressed ^ caps_lock) : right_shift_pressed;
        c = use_upper ? scancode_ascii_upper[scancode] : base;
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

void kbd_set_leds(uint8_t led_mask) {
#if !defined(__riscv)
    // Wait until 8042 controller input buffer is clear
    for (int i = 0; i < 10000; i++) {
        if (!(inb(0x64) & 2)) break;
    }
    // Command 0xED: Set/Reset LEDs
    outb(0x60, 0xED);
    // Wait for ACK (0xFA) or timeout
    for (int i = 0; i < 10000; i++) {
        if (inb(0x64) & 1) {
            uint8_t ack = inb(0x60);
            if (ack == 0xFA) break;
        }
    }
    // Wait until controller input buffer is clear
    for (int i = 0; i < 10000; i++) {
        if (!(inb(0x64) & 2)) break;
    }
    // Send LED state byte: bit 0 = ScrollLock, bit 1 = NumLock, bit 2 = CapsLock
    outb(0x60, (uint8_t)(led_mask & 0x07));
    // Drain controller ACK
    for (int i = 0; i < 10000; i++) {
        if (inb(0x64) & 1) {
            inb(0x60);
            break;
        }
    }
#else
    (void)led_mask;
#endif
}

void kbd_blackout_leds(void) {
    // Extinguish all keyboard LEDs to deny optical sensors & photodiodes
    kbd_set_leds(0x00);
    serial_puts("[+] OpSec: Keyboard LEDs extinguished (Blackout 0x00).\n");
}

