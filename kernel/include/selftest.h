/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Subsystem Self-Test Suite Header
 * GPLv3 Licensed.
 */

#ifndef SELD_SELFTEST_H
#define SELD_SELFTEST_H

#include <stdint.h>
#include <stddef.h>

int selftest_run_all(void);
int selftest_kmalloc(void);
int selftest_sha256(void);
int selftest_pmm(void);
int selftest_scheduler(void);
int selftest_spinlock(void);
int selftest_user_buffer(void);
int selftest_net(void);

#endif /* SELD_SELFTEST_H */
