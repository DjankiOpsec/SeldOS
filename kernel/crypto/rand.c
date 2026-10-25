/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Zero-Trust Hardware Entropy & CSPRNG Implementation
 * Whitens CPU execution jitter and untrusted hardware feeds via SHA-256.
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "rand.h"
#include "serial.h"
#include "sha256.h"
#include "string.h"

static int rdrand_available = 0;
static uint64_t fallback_state = 0x5E1DCAFE5E1DCAFEULL;
static uint64_t s_entropy_counter = 0;
static uint8_t  s_entropy_pool[SHA256_DIGEST_SIZE] = {0};

static inline uint64_t rdtsc(void) {
#if defined(__riscv)
    uint64_t val;
    __asm__ volatile ("rdtime %0" : "=r"(val));
    return val;
#else
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#endif
}

void rng_init(void) {
#if defined(__riscv)
    rdrand_available = 0;
    serial_puts("[+] Crypto: RISC-V RDTIME timer entropy + Zero-Trust SHA-256 CSPRNG active.\n");
#else
    uint32_t eax, ebx, ecx = 0, edx;
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );

    // ECX bit 30 indicates RDRAND support
    if (ecx & (1 << 30)) {
        rdrand_available = 1;
        serial_puts("[+] Crypto: CPU RDRAND detected [Tainted Hardware - Whitening with SHA-256].\n");
    } else {
        rdrand_available = 0;
        serial_puts("[+] Crypto: CPU RDRAND not present, using RDTSC Timer Jitter CSPRNG.\n");
    }
#endif

    fallback_state ^= rdtsc();

    // Seed initial entropy pool with multiple uncorrelated runtime metrics
    struct {
        uint64_t tsc;
        uint64_t state;
        uintptr_t stack_loc;
    } init_feed;
    init_feed.tsc = rdtsc();
    init_feed.state = fallback_state;
    init_feed.stack_loc = (uintptr_t)&init_feed;

    sha256_hash(&init_feed, sizeof(init_feed), s_entropy_pool);
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
#if defined(__riscv)
    uint64_t sstatus;
    __asm__ volatile ("csrr %0, sstatus; csrci sstatus, 2" : "=r"(sstatus) : : "memory");
#else
    uint64_t rflags;
    __asm__ volatile (
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(rflags) : : "memory"
    );
#endif

    uint64_t raw_cpu_rdrand = 0;
#if !defined(__riscv)
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
                raw_cpu_rdrand = val;
                break;
            }
        }
    }
#endif

    /*
     * Zero-Trust Entropy Whitening Sponge:
     * Never trust CPU RDRAND directly! Even if on-die hardware contains
     * proprietary backdoors or NSA/vendor trapdoors, we sponge-mix the raw
     * feed with cycle counters, memory layout jitter, SplitMix64 state,
     * and previous digest, then pass through cryptographic SHA-256.
     */
    struct {
        uint8_t   prev_pool[SHA256_DIGEST_SIZE];
        uint64_t  tsc_sample;
        uint64_t  raw_rdrand;
        uint64_t  prng_sample;
        uint64_t  counter;
        uintptr_t stack_entropy;
    } mix_input;

    memcpy(mix_input.prev_pool, s_entropy_pool, SHA256_DIGEST_SIZE);
    mix_input.tsc_sample = rdtsc();
    mix_input.raw_rdrand = raw_cpu_rdrand;
    fallback_state ^= mix_input.tsc_sample;
    mix_input.prng_sample = splitmix64(&fallback_state);
    mix_input.counter = ++s_entropy_counter;
    mix_input.stack_entropy = (uintptr_t)&mix_input;

    sha256_hash(&mix_input, sizeof(mix_input), s_entropy_pool);

    uint64_t result;
    memcpy(&result, s_entropy_pool, sizeof(uint64_t));

#if defined(__riscv)
    if (sstatus & 2) {
        __asm__ volatile ("csrsi sstatus, 2" ::: "memory");
    }
#else
    if (rflags & 0x200) {
        __asm__ volatile ("sti" ::: "memory");
    }
#endif

    return result;
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
