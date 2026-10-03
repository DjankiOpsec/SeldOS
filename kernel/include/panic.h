/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Panic Subsystem Header
 * Hardware fault reporting, register dump, and system halt.
 * GPLv3 Licensed.
 */

#ifndef SELD_PANIC_H
#define SELD_PANIC_H

#include <stdint.h>
#include <stddef.h>
#include "idt.h"

void kernel_panic_diagnostic(const char* file, int line, const char* func, const char* reason) __attribute__((noreturn));
void kernel_panic(const char* reason) __attribute__((noreturn));
void kernel_panic_exception(uint8_t vector, uint64_t err_code, struct interrupt_frame* frame) __attribute__((noreturn));
int kernel_panic_in_progress(void);

#define kernel_panic(reason) kernel_panic_diagnostic(__FILE__, __LINE__, __func__, (reason))

#endif /* SELD_PANIC_H */
