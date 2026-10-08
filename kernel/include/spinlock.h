/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Spinlock Primitives Implementation
 * Hardware-enforced mutual exclusion via GCC atomics and x86 PAUSE.
 * GPLv3 Licensed.
 */

#ifndef SELD_SPINLOCK_H
#define SELD_SPINLOCK_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spin_init(spinlock_t* lock) {
    if (!lock) return;
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
}

static inline int spin_trylock(spinlock_t* lock) {
    if (!lock) return 0;
    // Returns 1 if lock was acquired, 0 if already locked
    return !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE);
}

static inline void spin_lock(spinlock_t* lock) {
    if (!lock) return;
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED)) {
#if defined(__riscv)
            __asm__ volatile ("nop");
#else
            __builtin_ia32_pause();
#endif
        }
    }
}

static inline void spin_unlock(spinlock_t* lock) {
    if (!lock) return;
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

static inline uint64_t spin_lock_irqsave(spinlock_t* lock) {
#if defined(__riscv)
    uint64_t sstatus;
    __asm__ volatile ("csrrci %0, sstatus, 2" : "=r"(sstatus) : : "memory");
    spin_lock(lock);
    return sstatus;
#else
    uint64_t rflags;
    __asm__ volatile (
        "pushfq\n\t"
        "pop %0\n\t"
        "cli"
        : "=r"(rflags)
        :
        : "memory"
    );
    spin_lock(lock);
    return rflags;
#endif
}

static inline void spin_unlock_irqrestore(spinlock_t* lock, uint64_t flags) {
    spin_unlock(lock);
#if defined(__riscv)
    if (flags & 2) {
        __asm__ volatile ("csrsi sstatus, 2" : : : "memory");
    }
#else
    __asm__ volatile (
        "push %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "memory"
    );
#endif
}

#endif /* SELD_SPINLOCK_H */
