#ifndef SELD_SERIAL_H
#define SELD_SERIAL_H

#include <stdint.h>
#include <stddef.h>

void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char* str);
void serial_print_hex(uint64_t val);
void serial_print_dec(uint64_t val);
int serial_has_char(void);
char serial_getchar(void);

#endif
