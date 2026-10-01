/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Hardware Entropy & RNG Implementation
 * Uses hardware RDRAND (Intel/AMD Ivy Bridge+) with fallback SplitMix64 PRNG.
 * GPLv3 Licensed.
 */

#include "rand.h"
#include "serial.h"

static int rdrand_available = 0;
static uint64_t fallback_state = 0x5E1DCAFE5E1DCAFEULL;

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void rng_init(void) {
    uint32_t eax, ebx, ecx = 0, edx;
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );

    // ECX bit 30 indicates RDRAND support
    if (ecx & (1 << 30)) {
        rdrand_available = 1;
        serial_puts("[+] Crypto: Hardware RDRAND instruction detected and active.\n");
    } else {
        rdrand_available = 0;
        serial_puts("[!] Crypto: RDRAND not present, falling back to RDTSC SplitMix64 generator.\n");
    }

    fallback_state ^= rdtsc();
}

int rng_has_rdrand(void) {
    return rdrand_available;
}

static uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint64_t rng_get_u64(void) {
    if (rdrand_available) {
        uint64_t val;
        unsigned char ok;
        for (int retry = 0; retry < 10; retry++) {
            __asm__ volatile (
                "rdrand %0\n"
                "setc %1\n"
                : "=r"(val), "=qm"(ok)
            );
            if (ok) {
                return val;
            }
        }
    }

    // Fallback: entropy from cycle counter + SplitMix64
    fallback_state ^= rdtsc();
    return splitmix64(&fallback_state);
}

void rng_get_bytes(void* dest, size_t len) {
    uint8_t* p = (uint8_t*)dest;
    while (len >= 8) {
        uint64_t r = rng_get_u64();
        *(uint64_t*)p = r;
        p += 8;
        len -= 8;
    }
    if (len > 0) {
        uint64_t r = rng_get_u64();
        for (size_t i = 0; i < len; i++) {
            p[i] = (uint8_t)(r >> (i * 8));
        }
    }
}
