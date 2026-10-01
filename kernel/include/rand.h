/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Hardware Entropy & RNG Interface
 * Support for CPU RDRAND instruction with fallback jitter entropy.
 * GPLv3 Licensed.
 */

#ifndef SELD_RAND_H
#define SELD_RAND_H

#include <stdint.h>
#include <stddef.h>

void rng_init(void);
int rng_has_rdrand(void);
uint64_t rng_get_u64(void);
void rng_get_bytes(void* dest, size_t len);

#endif /* SELD_RAND_H */
