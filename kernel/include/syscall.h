#ifndef SELD_SYSCALL_H
#define SELD_SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "idt.h"

#define SYS_EXIT   1
#define SYS_WRITE  2
#define SYS_READ   3
#define SYS_SELD   42  // Special SeldOS Humboldt ping/ident

int validate_user_buffer(const void* user_ptr, size_t size, int write);
void syscall_dispatch(struct interrupt_frame* frame);

#endif
