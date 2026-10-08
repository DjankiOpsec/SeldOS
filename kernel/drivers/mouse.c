/*
 * SeldOS - Humboldt Kernel Project
 * PS/2 Mouse & Touchscreen Subsystem Driver
 * GPLv3 Licensed.
 */

#include "mouse.h"
#include "io.h"
#include "serial.h"
#include "pit.h"

#define MOUSE_QUEUE_SIZE 128

static struct mouse_event s_mouse_queue[MOUSE_QUEUE_SIZE];
static volatile size_t s_q_head = 0;
static volatile size_t s_q_tail = 0;

static int32_t s_mouse_x = 340;
static int32_t s_mouse_y = 167;
static uint8_t s_mouse_buttons = 0;

static uint8_t s_mouse_cycle = 0;
static uint8_t s_mouse_packet[4];
static uint64_t s_last_byte_time_ms = 0;

static inline void mouse_wait_write(void) {
    int timeout = 50000;
    while (timeout-- && (inb(0x64) & 2)) {
        io_wait();
    }
}

static inline void mouse_wait_read(void) {
    int timeout = 50000;
    while (timeout-- && !(inb(0x64) & 1)) {
        io_wait();
    }
}

static void mouse_write(uint8_t write_val) {
    mouse_wait_write();
    outb(0x64, 0xD4); // Route to auxiliary/mouse port
    mouse_wait_write();
    outb(0x60, write_val);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    if (inb(0x64) & 1) {
        return inb(0x60);
    }
    return 0;
}

void mouse_init(void) {
#if defined(__riscv)
    __asm__ volatile ("csrci sstatus, 2");
#else
    __asm__ volatile ("cli");
#endif
    s_q_head = 0;
    s_q_tail = 0;
    s_mouse_x = 340;
    s_mouse_y = 167;
    s_mouse_buttons = 0;
    s_mouse_cycle = 0;
    s_last_byte_time_ms = 0;

    // Flush any leftover bytes from BIOS/GRUB
    int flush = 1000;
    while (flush-- && (inb(0x64) & 1)) {
        inb(0x60);
        io_wait();
    }

    // 1. Enable auxiliary mouse port on 8042 controller
    mouse_wait_write();
    outb(0x64, 0xA8);

    // 2. Read Controller Command Byte
    mouse_wait_write();
    outb(0x64, 0x20);
    uint8_t status = mouse_read();

    // Enable IRQ12 (bit 1) and enable mouse clock (clear bit 5)
    status |= 0x02;
    status &= ~0x20;

    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    // 3. Reset / Set defaults
    mouse_write(0xF6);
    (void)mouse_read(); // ACK 0xFA

    // 4. Enable data reporting
    mouse_write(0xF4);
    (void)mouse_read(); // ACK 0xFA

    // Post-init flush
    flush = 1000;
    while (flush-- && (inb(0x64) & 1)) {
        inb(0x60);
        io_wait();
    }

    serial_puts("[+] Mouse: Universal PS/2 Mouse & Touch Driver initialized.\n");
#if defined(__riscv)
    __asm__ volatile ("csrsi sstatus, 2");
#else
    __asm__ volatile ("sti");
#endif
}

void mouse_handle_byte(uint8_t data) {
    uint64_t now = pit_get_uptime_ms();
    // Resynchronize cycle if gap between bytes is larger than standard burst duration
    if (s_last_byte_time_ms > 0 && (now - s_last_byte_time_ms > 40)) {
        s_mouse_cycle = 0;
    }
    s_last_byte_time_ms = now;

    if (s_mouse_cycle == 0) {
        // Bit 3 MUST be 1 on byte 0 of standard PS/2 packet
        if (!(data & 0x08)) {
            s_mouse_cycle = 0;
            return;
        }
        s_mouse_packet[0] = data;
        s_mouse_cycle = 1;
    } else if (s_mouse_cycle == 1) {
        s_mouse_packet[1] = data;
        s_mouse_cycle = 2;
    } else if (s_mouse_cycle == 2) {
        s_mouse_packet[2] = data;
        s_mouse_cycle = 0;

        int32_t dx = (int32_t)s_mouse_packet[1];
        if (s_mouse_packet[0] & 0x10) dx |= 0xFFFFFF00;

        int32_t dy = (int32_t)s_mouse_packet[2];
        if (s_mouse_packet[0] & 0x20) dy |= 0xFFFFFF00;

        // Overflow clamping
        if (s_mouse_packet[0] & 0x40) dx = (s_mouse_packet[0] & 0x10) ? -255 : 255;
        if (s_mouse_packet[0] & 0x80) dy = (s_mouse_packet[0] & 0x20) ? -255 : 255;

        s_mouse_x += dx;
        s_mouse_y -= dy; // Invert dy for display coordinates

        if (s_mouse_x < 0) s_mouse_x = 0;
        if (s_mouse_x >= 680) s_mouse_x = 679;
        if (s_mouse_y < 0) s_mouse_y = 0;
        if (s_mouse_y >= 334) s_mouse_y = 333;

        s_mouse_buttons = s_mouse_packet[0] & 0x07;

        size_t next_head = (s_q_head + 1) % MOUSE_QUEUE_SIZE;
        if (next_head != s_q_tail) {
            s_mouse_queue[s_q_head].x = s_mouse_x;
            s_mouse_queue[s_q_head].y = s_mouse_y;
            s_mouse_queue[s_q_head].dx = dx;
            s_mouse_queue[s_q_head].dy = dy;
            s_mouse_queue[s_q_head].buttons = s_mouse_buttons;
            s_q_head = next_head;
        }
    }
}

void mouse_handle_irq(void) {
    while (inb(0x64) & 1) {
        uint8_t status = inb(0x64);
        uint8_t data = inb(0x60);
        if (status & 0x20) {
            mouse_handle_byte(data);
        }
    }
}

int mouse_poll_event(struct mouse_event* ev) {
    if (s_q_head == s_q_tail) {
        return 0;
    }
    *ev = s_mouse_queue[s_q_tail];
    s_q_tail = (s_q_tail + 1) % MOUSE_QUEUE_SIZE;
    return 1;
}

void mouse_get_state(int32_t* x, int32_t* y, uint8_t* buttons) {
    if (x) *x = s_mouse_x;
    if (y) *y = s_mouse_y;
    if (buttons) *buttons = s_mouse_buttons;
}
