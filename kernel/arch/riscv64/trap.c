/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * RISC-V 64-bit Supervisor Trap Handling & Syscall Vector Dispatcher
 * GPLv3 Licensed.
 */

#include <stdint.h>
#include <stddef.h>
#include "vmm.h"
#include "serial.h"
#include "fast_syscall.h"
#include "pit.h"

struct trap_frame {
    uint64_t ra;      // 0*8
    uint64_t sp;      // 1*8
    uint64_t gp;      // 2*8
    uint64_t tp;      // 3*8
    uint64_t t0;      // 4*8
    uint64_t t1;      // 5*8
    uint64_t t2;      // 6*8
    uint64_t s0;      // 7*8 (fp)
    uint64_t s1;      // 8*8
    uint64_t a0;      // 9*8
    uint64_t a1;      // 10*8
    uint64_t a2;      // 11*8
    uint64_t a3;      // 12*8
    uint64_t a4;      // 13*8
    uint64_t a5;      // 14*8
    uint64_t a6;      // 15*8
    uint64_t a7;      // 16*8
    uint64_t s2;      // 17*8
    uint64_t s3;      // 18*8
    uint64_t s4;      // 19*8
    uint64_t s5;      // 20*8
    uint64_t s6;      // 21*8
    uint64_t s7;      // 22*8
    uint64_t s8;      // 23*8
    uint64_t s9;      // 24*8
    uint64_t s10;     // 25*8
    uint64_t s11;     // 26*8
    uint64_t t3;      // 27*8
    uint64_t t4;      // 28*8
    uint64_t t5;      // 29*8
    uint64_t t6;      // 30*8
    uint64_t sstatus; // 31*8
    uint64_t sepc;    // 32*8
    uint64_t scause;  // 33*8
    uint64_t stval;   // 34*8
};

extern void trap_entry_asm(void);
uint64_t fast_syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);

void riscv_trap_init(void) {
    uint64_t trap_addr = (uint64_t)trap_entry_asm;
    __asm__ volatile ("csrw stvec, %0" : : "r"(trap_addr) : "memory");
    __asm__ volatile ("csrsi sstatus, 2" : : : "memory");
}

void riscv_trap_handler(struct trap_frame* tf) {
    // Check if interrupt (MSB set)
    if (tf->scause & (1ULL << 63)) {
        uint64_t irq = tf->scause & 0x7FFFFFFFFFFFFFFF;
        if (irq == 5) { // Supervisor timer interrupt
            pit_handle_interrupt();
        }
        return;
    }

    // Synchronous exception
    uint64_t ex = tf->scause;


    if (ex == 8) {
        // Environment call from U-mode (User Syscall)
        tf->sepc += 4; // Advance past the ecall instruction
        uint64_t ret = fast_syscall_dispatcher(tf->a7, tf->a0, tf->a1, tf->a2, tf->a3);
        tf->a0 = ret;
        return;
    }

    // Unhandled exception diagnostic output
    serial_puts("\n[!] FATAL CPU EXCEPTION: scause=0x");
    serial_print_hex(tf->scause);
    serial_puts(" sepc=0x");
    serial_print_hex(tf->sepc);
    serial_puts(" stval=0x");
    serial_print_hex(tf->stval);
    serial_puts(" sstatus=0x");
    serial_print_hex(tf->sstatus);
    serial_puts("\n");

    // If fault occurred in U-mode (SPP == 0), terminate the user process cleanly
    if (!(tf->sstatus & (1 << 8))) {
        serial_puts("[-] Terminating Ring 3 user process due to unhandled fault.\n");
        user_exit_to_kernel(128 + ex);
    }

    // Supervisor kernel fault: halt hardware
    serial_puts("[!] KERNEL PANIC: Unrecoverable Supervisor Fault\n");
    while (1) {
        __asm__ volatile ("wfi");
    }
}

/* Stubs for architecture-independent symbol tables & drivers */
void gdt_init(void) {}
void idt_init(void) {}
void idt_set_gate(void) {}
void pic_unmask_irq(uint8_t irq) { (void)irq; }
void pic_mask_irq(uint8_t irq) { (void)irq; }
void isr_handler(void) {}
void irq_register_handler(uint8_t irq, void* h) { (void)irq; (void)h; }
void tss_set_rsp0(uint64_t rsp0) { (void)rsp0; }
