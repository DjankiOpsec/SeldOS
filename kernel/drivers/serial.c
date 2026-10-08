#include "serial.h"
#include "io.h"

#if defined(__riscv)
#include "vmm.h"
#define UART_PHYS 0x10000000ULL

static volatile uint8_t* uart_reg(int reg) {
    uint64_t sp;
    __asm__ volatile ("mv %0, sp" : "=r"(sp));
    uint64_t base = UART_PHYS;
    if (sp >= 0xFFFFFF0000000000ULL) {
        base += HHDM_VIRT_OFFSET;
    }
    return (volatile uint8_t*)(base + reg);
}

void serial_init(void) {
    *uart_reg(1) = 0x00; // Disable all interrupts
    *uart_reg(3) = 0x80; // Enable DLAB (divisor)
    *uart_reg(0) = 0x03; // Divisor lo (38400 baud)
    *uart_reg(1) = 0x00; // Divisor hi
    *uart_reg(3) = 0x03; // 8 bits, no parity, 1 stop bit
    *uart_reg(2) = 0xC7; // Enable FIFO
    *uart_reg(4) = 0x0B; // RTS/DSR set
}

static int is_transmit_empty(void) {
    return *uart_reg(5) & 0x20;
}

void serial_putchar(char c) {
    while (is_transmit_empty() == 0);
    *uart_reg(0) = (uint8_t)c;
}

int serial_has_char(void) {
    return *uart_reg(5) & 1;
}

char serial_getchar(void) {
    while (!serial_has_char());
    return (char)*uart_reg(0);
}

#else

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static int is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (is_transmit_empty() == 0);
    outb(COM1, c);
}

int serial_has_char(void) {
    return inb(COM1 + 5) & 1;
}

char serial_getchar(void) {
    while (!serial_has_char());
    return (char)inb(COM1);
}

#endif

void serial_puts(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

void serial_print_hex(uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putchar(hex_chars[(val >> i) & 0xF]);
    }
}

void serial_print_dec(uint64_t val) {
    if (val == 0) {
        serial_putchar('0');
        return;
    }
    char buf[32];
    int i = 0;
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (i > 0) {
        serial_putchar(buf[--i]);
    }
}
