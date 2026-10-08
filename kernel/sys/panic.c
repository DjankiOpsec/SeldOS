/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Panic Subsystem Implementation
 * Real Linux-style kernel panic with compiler diagnostic error formatting,
 * exact problem and location reporting, register dump, stack unwinding,
 * acoustic alerts, and hardware halt.
 * GPLv3 Licensed.
 */

#include "panic.h"
#include "vga.h"
#include "serial.h"
#include "io.h"
#include "pit.h"
#include "string.h"
#include "ksyms.h"
#include "fast_syscall.h"

#undef kernel_panic

static volatile int s_panic_in_progress = 0;

int kernel_panic_in_progress(void) {
    return s_panic_in_progress;
}

static void panic_beep(void) {
    // Play a brief 440 Hz alert tone directly on PC Speaker (PIT channel 2)
    uint32_t div = 1193180 / 440;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t prev = inb(0x61);
    outb(0x61, prev | 3);
    for (volatile int i = 0; i < 4000000; i++) {
        io_wait();
    }
    outb(0x61, prev & ~3);
}

static void panic_flash_leds(void) {
    // Command keyboard controller to flash Caps Lock / Scroll Lock / Num Lock
    outb(0x60, 0xED);
    io_wait();
    outb(0x60, 0x07);
}

static const char* exception_name(uint8_t vector) {
    switch (vector) {
        case 0:  return "#DE Divide-by-zero Error";
        case 1:  return "#DB Debug Exception";
        case 2:  return "#NMI Non-Maskable Interrupt";
        case 3:  return "#BP Breakpoint";
        case 4:  return "#OF Overflow";
        case 5:  return "#BR Bound Range Exceeded";
        case 6:  return "#UD Invalid Opcode";
        case 7:  return "#NM Device Not Available";
        case 8:  return "#DF Double Fault";
        case 10: return "#TS Invalid TSS";
        case 11: return "#NP Segment Not Present";
        case 12: return "#SS Stack-Segment Fault";
        case 13: return "#GP General Protection Fault";
        case 14: return "#PF Page Fault";
        case 16: return "#MF x87 FPU Floating-Point Error";
        case 17: return "#AC Alignment Check";
        case 18: return "#MC Machine Check";
        case 19: return "#XM SIMD Floating-Point Exception";
        case 20: return "#VE Virtualization Exception";
        case 21: return "#CP Control Protection Exception";
        default: return "Hardware Trap / Unknown Fault";
    }
}


static void panic_strlcpy(char* dst, const char* src, size_t max) {
    if (!dst || max == 0) return;
    size_t i = 0;
    if (src) {
        while (src[i] && i + 1 < max) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void panic_strlcat(char* dst, const char* src, size_t max) {
    if (!dst || max == 0) return;
    size_t dlen = 0;
    while (dst[dlen] && dlen < max) dlen++;
    if (dlen >= max) return;
    size_t i = 0;
    if (src) {
        while (src[i] && dlen + i + 1 < max) {
            dst[dlen + i] = src[i];
            i++;
        }
    }
    dst[dlen + i] = '\0';
}

static void hex_to_str(uint64_t val, char* buf) {
    const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[2 + i] = hex[(val >> ((15 - i) * 4)) & 0xF];
    }
    buf[18] = '\0';
}

static void dec_to_str(uint64_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[24];
    int pos = 0;
    while (val > 0) {
        temp[pos++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = 0; i < pos; i++) {
        buf[i] = temp[pos - 1 - i];
    }
    buf[pos] = '\0';
}

static int is_valid_code_addr(uint64_t addr) {
    if (addr >= 0xFFFFFFFF80000000ULL && addr < 0xFFFFFFFF80300000ULL) return 1;
    if (addr >= 0x0000000000400000ULL && addr < 0x0000800000000000ULL) return 1;
    return 0;
}

static int is_valid_stack_addr(uint64_t addr) {
    if ((addr & 7) != 0) return 0;
    if (addr >= 0xFFFF800000000000ULL && addr <= 0xFFFFFFFFFFFFFFF8ULL) return 1;
    if (addr >= 0x0000000000010000ULL && addr < 0x0000800000000000ULL) return 1;
    return 0;
}

static void panic_reboot(void) __attribute__((noreturn));
static void panic_reboot(void) {
#if defined(__riscv)
    fast_sys_reboot();
    while (1) {
        __asm__ volatile ("wfi");
    }
#else
    // 1. 8042 keyboard controller reset pulse
    for (int t = 0; t < 1000; t++) {
        uint8_t temp = inb(0x64);
        if (temp & 1) inb(0x60);
        if (!(temp & 2)) break;
    }
    outb(0x64, 0xFE);

    // 2. ACPI/PCI hard reset port 0xCF9
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    outb(0xCF9, 0x0E);

    // 3. Fast reset port 0x92
    outb(0x92, (uint8_t)(inb(0x92) | 1));

    // 4. Triple fault via corrupted IDT
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile ("lidt %0; int $3" : : "m"(null_idt));

    while (1) {
        __asm__ volatile ("hlt");
    }
#endif
}

static int panic_draw_problem_wrapped(int start_row, const char* reason, uint8_t color) {
    if (!reason) {
        vga_draw_string_at(2, start_row, "    fatal kernel fault", color);
        return start_row + 1;
    }
    size_t len = strlen(reason);
    if (len <= 76) {
        char buf[86];
        buf[0] = '\0';
        panic_strlcpy(buf, "    ", sizeof(buf));
        panic_strlcat(buf, reason, sizeof(buf));
        vga_draw_string_at(2, start_row, buf, color);
        return start_row + 1;
    }
    size_t split = 76;
    for (size_t i = 76; i > 25; i--) {
        if (reason[i] == ' ') {
            split = i;
            break;
        }
    }
    char line1[86];
    size_t c1 = split < 80 ? split : 79;
    memcpy(line1, "    ", 4);
    memcpy(line1 + 4, reason, c1);
    line1[4 + c1] = '\0';
    vga_draw_string_at(2, start_row, line1, color);

    const char* rem = reason + split;
    while (*rem == ' ') rem++;
    char line2[86];
    line2[0] = '\0';
    panic_strlcpy(line2, "    ", sizeof(line2));
    panic_strlcat(line2, rem, sizeof(line2));
    vga_draw_string_at(2, start_row + 1, line2, color);
    return start_row + 2;
}

static void kernel_panic_extended(const char* file, int line, const char* func, const char* reason, struct interrupt_frame* frame, void* caller_rip) __attribute__((noreturn));

static void kernel_panic_extended(const char* file, int line, const char* func, const char* reason, struct interrupt_frame* frame, void* caller_rip) {
#if defined(__riscv)
    __asm__ volatile ("csrci sstatus, 2");
#else
    __asm__ volatile ("cli");
#endif
    s_panic_in_progress = 1;

    uint64_t rax=0, rbx=0, rcx=0, rdx=0, rsi=0, rdi=0, rbp=0, rsp=0;
    uint64_t r8=0, r9=0, r10=0, r11=0, r12=0, r13=0, r14=0, r15=0;
    uint64_t rflags = 0;
    void* rip = NULL;

#if defined(__riscv)
    (void)frame;
    rip = caller_rip ? caller_rip : __builtin_return_address(0);
#else
    if (frame != NULL) {
        rax = frame->rax;
        rbx = frame->rbx;
        rcx = frame->rcx;
        rdx = frame->rdx;
        rsi = frame->rsi;
        rdi = frame->rdi;
        rbp = frame->rbp;
        rsp = frame->rsp;
        r8  = frame->r8;
        r9  = frame->r9;
        r10 = frame->r10;
        r11 = frame->r11;
        r12 = frame->r12;
        r13 = frame->r13;
        r14 = frame->r14;
        r15 = frame->r15;
        rflags = frame->rflags;
        rip = (void*)frame->rip;
    } else {
        __asm__ volatile ("mov %%rax, %0" : "=r"(rax));
        __asm__ volatile ("mov %%rbx, %0" : "=r"(rbx));
        __asm__ volatile ("mov %%rcx, %0" : "=r"(rcx));
        __asm__ volatile ("mov %%rdx, %0" : "=r"(rdx));
        __asm__ volatile ("mov %%rsi, %0" : "=r"(rsi));
        __asm__ volatile ("mov %%rdi, %0" : "=r"(rdi));
        __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
        __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
        __asm__ volatile ("mov %%r8,  %0" : "=r"(r8));
        __asm__ volatile ("mov %%r9,  %0" : "=r"(r9));
        __asm__ volatile ("mov %%r10, %0" : "=r"(r10));
        __asm__ volatile ("mov %%r11, %0" : "=r"(r11));
        __asm__ volatile ("mov %%r12, %0" : "=r"(r12));
        __asm__ volatile ("mov %%r13, %0" : "=r"(r13));
        __asm__ volatile ("mov %%r14, %0" : "=r"(r14));
        __asm__ volatile ("mov %%r15, %0" : "=r"(r15));
        __asm__ volatile ("pushfq; pop %0" : "=r"(rflags));
        rip = caller_rip ? caller_rip : __builtin_return_address(0);
    }
#endif

    uint64_t cr0 = 0, cr2 = 0, cr3 = 0, cr4 = 0;
#if defined(__riscv)
    __asm__ volatile ("csrr %0, sstatus" : "=r"(cr0));
    __asm__ volatile ("csrr %0, stval"   : "=r"(cr2));
    __asm__ volatile ("csrr %0, satp"    : "=r"(cr3));
    __asm__ volatile ("csrr %0, scause"  : "=r"(cr4));
#else
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
#endif

    uint64_t uptime_ms = pit_get_uptime_ms();

    // Resolve symbol for RIP if function or location is not specified
    uint64_t sym_offset = 0;
    const struct kernel_symbol* sym = ksym_lookup((uint64_t)rip, &sym_offset);
    if (!func && sym) {
        func = sym->name;
    }
    if (!file && sym) {
        file = sym->file;
        line = sym->line;
    }
    if (!file) {
        if (frame != NULL && (frame->cs & 3) == 3) {
            file = "userspace";
        } else {
            file = "<kernel>";
        }
    }
    if (!func) {
        func = "kernel_panic";
    }

    // Build compiler diagnostic strings
    char line_str[16];
    dec_to_str(line > 0 ? line : 0, line_str);

    // 1. diag_loc: "kernel/sched/sched.c:139: error:"
    char diag_loc[96];
    diag_loc[0] = '\0';
    panic_strlcpy(diag_loc, file, sizeof(diag_loc));
    if (line > 0) {
        panic_strlcat(diag_loc, ":", sizeof(diag_loc));
        panic_strlcat(diag_loc, line_str, sizeof(diag_loc));
    }
    panic_strlcat(diag_loc, ": error:", sizeof(diag_loc));

    // 2. Full compiler diagnostic message:
    // "kernel/sched/sched.c:139: error: in 'sched_tick': quantum preempt tick dropped (s_cpu_drv_enabled == 0)"
    char full_diagnostic[320];
    full_diagnostic[0] = '\0';
    panic_strlcpy(full_diagnostic, diag_loc, sizeof(full_diagnostic));
    panic_strlcat(full_diagnostic, " in '", sizeof(full_diagnostic));
    panic_strlcat(full_diagnostic, func, sizeof(full_diagnostic));
    panic_strlcat(full_diagnostic, "': ", sizeof(full_diagnostic));
    panic_strlcat(full_diagnostic, reason ? reason : "fatal kernel fault", sizeof(full_diagnostic));

    // 1. Acoustic and hardware alerts
    panic_beep();
    panic_flash_leds();

    // 2. Comprehensive Serial Log Output (Compiler Diagnostic & Linux-Style)
    serial_puts("\n================================================================================\n");
    serial_puts(" [!] KERNEL PANIC: NOT SYNCING (Humboldt Kernel v0.1-sec - GPLv3)\n");
    serial_puts("================================================================================\n");
    serial_puts(" ");
    serial_puts(full_diagnostic);
    serial_puts("\n\n");

    serial_puts("   --> ");
    serial_puts(diag_loc);
    serial_puts("\n");
    serial_puts("    | Location:         ");
    serial_puts(file);
    if (line > 0) {
        serial_puts(":");
        serial_puts(line_str);
    }
    serial_puts("\n");
    serial_puts("    | Symbol/Function:  ");
    serial_puts(func);
    if (sym_offset > 0) {
        serial_puts("+");
        serial_print_hex(sym_offset);
    }
    serial_puts("\n");
    serial_puts("    | Instruction RIP:  ");
    serial_print_hex((uint64_t)rip);
    serial_puts("\n");
    if (cr2 != 0) {
        serial_puts("    | Faulting Address: CR2 ");
        serial_print_hex(cr2);
        serial_puts("\n");
    }
    serial_puts("    | Faulting Reason:  ");
    serial_puts(reason ? reason : "fatal fault");
    serial_puts("\n\n");

    serial_puts(" Kernel panic - not syncing: ");
    serial_puts(full_diagnostic);
    serial_puts("\n\n");

    serial_puts(" CPU: 0 PID: 1 Comm: supervisor/kernel_main\n");
    serial_puts(" Hardware: Humboldt x86_64 Long Mode Supervisor Kernel\n");
    serial_puts(" System Uptime: ");
    serial_print_dec(uptime_ms);
    serial_puts(" ms\n\n");

    serial_puts(" [!] Register Dump:\n");
    serial_puts("   RIP: "); serial_print_hex((uint64_t)rip);
    serial_puts("   RSP: "); serial_print_hex(rsp);
    serial_puts("   RFLAGS: "); serial_print_hex(rflags);
    serial_puts("\n");

    serial_puts("   RAX: "); serial_print_hex(rax);
    serial_puts("   RBX: "); serial_print_hex(rbx);
    serial_puts("   RCX: "); serial_print_hex(rcx);
    serial_puts("   RDX: "); serial_print_hex(rdx);
    serial_puts("\n");

    serial_puts("   RSI: "); serial_print_hex(rsi);
    serial_puts("   RDI: "); serial_print_hex(rdi);
    serial_puts("   RBP: "); serial_print_hex(rbp);
    serial_puts("\n");

    serial_puts("   R8:  "); serial_print_hex(r8);
    serial_puts("   R9:  "); serial_print_hex(r9);
    serial_puts("   R10: "); serial_print_hex(r10);
    serial_puts("   R11: "); serial_print_hex(r11);
    serial_puts("\n");

    serial_puts("   R12: "); serial_print_hex(r12);
    serial_puts("   R13: "); serial_print_hex(r13);
    serial_puts("   R14: "); serial_print_hex(r14);
    serial_puts("   R15: "); serial_print_hex(r15);
    serial_puts("\n");

    serial_puts("   CR0: "); serial_print_hex(cr0);
    serial_puts("   CR2: "); serial_print_hex(cr2);
    serial_puts("   CR3: "); serial_print_hex(cr3);
    serial_puts("   CR4: "); serial_print_hex(cr4);
    serial_puts("\n\n");

    serial_puts(" [!] Stack Trace (RBP Unwind):\n");
    if (caller_rip != NULL && is_valid_code_addr((uint64_t)caller_rip)) {
        uint64_t frame_offset = 0;
        const struct kernel_symbol* frame_sym = ksym_lookup((uint64_t)caller_rip, &frame_offset);
        serial_puts("   [<");
        serial_print_hex((uint64_t)caller_rip);
        serial_puts(">] ");
        if (frame_sym) {
            serial_puts(frame_sym->name);
            serial_puts("+");
            serial_print_hex(frame_offset);
            serial_puts(" (");
            serial_puts(frame_sym->file);
            serial_puts(")\n");
        } else {
            serial_puts("? code+0x0\n");
        }
    }

    uint64_t* curr_rbp = (uint64_t*)rbp;
    for (int frame_i = 0; frame_i < 6 && curr_rbp != NULL; frame_i++) {
        if (!is_valid_stack_addr((uint64_t)curr_rbp)) break;
        uint64_t next_rbp = curr_rbp[0];
        uint64_t frame_rip = curr_rbp[1];
        if (!is_valid_code_addr(frame_rip)) break;

        uint64_t frame_offset = 0;
        const struct kernel_symbol* frame_sym = ksym_lookup(frame_rip, &frame_offset);
        if (frame_rip == (uint64_t)caller_rip ||
            (frame_sym && (strcmp(frame_sym->name, "kernel_panic_extended") == 0 ||
                          strcmp(frame_sym->name, "kernel_panic_diagnostic") == 0 ||
                          strcmp(frame_sym->name, "kernel_panic") == 0))) {
            if (next_rbp <= (uint64_t)curr_rbp || next_rbp - (uint64_t)curr_rbp > 0x100000) break;
            curr_rbp = (uint64_t*)next_rbp;
            continue;
        }

        serial_puts("   [<");
        serial_print_hex(frame_rip);
        serial_puts(">] ");
        if (frame_sym) {
            serial_puts(frame_sym->name);
            serial_puts("+");
            serial_print_hex(frame_offset);
            serial_puts(" (");
            serial_puts(frame_sym->file);
            serial_puts(")\n");
        } else {
            serial_puts("? code+0x0\n");
        }

        if (next_rbp <= (uint64_t)curr_rbp || next_rbp - (uint64_t)curr_rbp > 0x100000) break;
        curr_rbp = (uint64_t*)next_rbp;
    }

    serial_puts("\n---[ end Kernel panic - not syncing: ");
    serial_puts(full_diagnostic);
    serial_puts(" ]---\n");
    serial_puts("---[ SYSTEM HALTED: Power off or reset hardware ]---\n");
    serial_puts("Press [ESC] to reboot.\n");
    serial_puts("================================================================================\n\n");

    // 3. Screen Output (Graphical Framebuffer or Emergency Text Fallback)
    if (vga_is_gpu_enabled() && vga_get_fb_ptr() != NULL) {
        // Red panic background filling whole 680x334 display
        vga_fill_rect(0, 0, 680, 334, 0x0018181A); // COLOR_PENGUIN_TUXEDO
        vga_fill_rect(0, 0, 680, 26, 0x00E0533C);  // COLOR_THERMAL_CORE Crimson Header
        vga_fill_rect(0, 318, 680, 16, 0x00E0533C); // Bottom alert footer

        vga_draw_string_at(16, 0, "[ *** KERNEL PANIC: NOT SYNCING - SELD OS CRASH *** ]", 15);

        // Row 2: Compiler diagnostic header prefix
        char scr_diag[86];
        scr_diag[0] = '\0';
        panic_strlcpy(scr_diag, "--> ", sizeof(scr_diag));
        panic_strlcat(scr_diag, diag_loc, sizeof(scr_diag));
        panic_strlcat(scr_diag, " in '", sizeof(scr_diag));
        panic_strlcat(scr_diag, func, sizeof(scr_diag));
        panic_strlcat(scr_diag, "':", sizeof(scr_diag));
        vga_draw_string_at(2, 2, scr_diag, 15);

        // Row 3: Concrete technical problem (wrapped across 1 or 2 rows)
        int next_row = panic_draw_problem_wrapped(3, reason ? reason : "fatal kernel fault", 14);

        // Location & instruction pointer
        char loc_rip_buf[86];
        char rip_hex[20];
        hex_to_str((uint64_t)rip, rip_hex);
        loc_rip_buf[0] = '\0';
        panic_strlcpy(loc_rip_buf, "Location:   ", sizeof(loc_rip_buf));
        panic_strlcat(loc_rip_buf, file, sizeof(loc_rip_buf));
        if (line > 0) {
            panic_strlcat(loc_rip_buf, ":", sizeof(loc_rip_buf));
            panic_strlcat(loc_rip_buf, line_str, sizeof(loc_rip_buf));
        }
        panic_strlcat(loc_rip_buf, " | RIP: ", sizeof(loc_rip_buf));
        panic_strlcat(loc_rip_buf, rip_hex, sizeof(loc_rip_buf));
        vga_draw_string_at(2, next_row++, loc_rip_buf, 11);

        // Fault address / Subsystem
        char sub_buf[86];
        sub_buf[0] = '\0';
        if (cr2 != 0) {
            char cr2_hex[20];
            hex_to_str(cr2, cr2_hex);
            panic_strlcpy(sub_buf, "Fault Addr: CR2 ", sizeof(sub_buf));
            panic_strlcat(sub_buf, cr2_hex, sizeof(sub_buf));
            panic_strlcat(sub_buf, " (Page Fault Linear Address)", sizeof(sub_buf));
        } else {
            panic_strlcpy(sub_buf, "Subsystem:  Ring 0 Supervisor Core | Uptime: ", sizeof(sub_buf));
            char up_str[16];
            dec_to_str(uptime_ms, up_str);
            panic_strlcat(sub_buf, up_str, sizeof(sub_buf));
            panic_strlcat(sub_buf, " ms", sizeof(sub_buf));
        }
        vga_draw_string_at(2, next_row++, sub_buf, 11);

        vga_draw_string_at(2, 7, "CPU Registers Dump:", 11);

        char hbuf[20];

        hex_to_str((uint64_t)rip, hbuf);
        vga_draw_string_at(4, 8, "RIP: ", 7);
        vga_draw_string_at(9, 8, hbuf, 15);

        hex_to_str(rsp, hbuf);
        vga_draw_string_at(30, 8, "RSP: ", 7);
        vga_draw_string_at(35, 8, hbuf, 15);

        hex_to_str(rflags, hbuf);
        vga_draw_string_at(56, 8, "RFL: ", 7);
        vga_draw_string_at(61, 8, hbuf, 15);

        hex_to_str(rax, hbuf);
        vga_draw_string_at(4, 9, "RAX: ", 7);
        vga_draw_string_at(9, 9, hbuf, 14);

        hex_to_str(rbx, hbuf);
        vga_draw_string_at(30, 9, "RBX: ", 7);
        vga_draw_string_at(35, 9, hbuf, 14);

        hex_to_str(rcx, hbuf);
        vga_draw_string_at(56, 9, "RCX: ", 7);
        vga_draw_string_at(61, 9, hbuf, 14);

        hex_to_str(rdx, hbuf);
        vga_draw_string_at(4, 10, "RDX: ", 7);
        vga_draw_string_at(9, 10, hbuf, 14);

        hex_to_str(rsi, hbuf);
        vga_draw_string_at(30, 10, "RSI: ", 7);
        vga_draw_string_at(35, 10, hbuf, 14);

        hex_to_str(rdi, hbuf);
        vga_draw_string_at(56, 10, "RDI: ", 7);
        vga_draw_string_at(61, 10, hbuf, 14);

        hex_to_str(rbp, hbuf);
        vga_draw_string_at(4, 11, "RBP: ", 7);
        vga_draw_string_at(9, 11, hbuf, 14);

        hex_to_str(cr0, hbuf);
        vga_draw_string_at(30, 11, "CR0: ", 7);
        vga_draw_string_at(35, 11, hbuf, 14);

        hex_to_str(cr3, hbuf);
        vga_draw_string_at(56, 11, "CR3: ", 7);
        vga_draw_string_at(61, 11, hbuf, 14);

        vga_draw_string_at(2, 13, "[+] Call Trace:", 10);
        int scr_row = 14;
        if (caller_rip != NULL && is_valid_code_addr((uint64_t)caller_rip) && scr_row < 16) {
            char scr_trace[80];
            char hbuf_rip[20];
            hex_to_str((uint64_t)caller_rip, hbuf_rip);
            uint64_t trace_off = 0;
            const struct kernel_symbol* trace_sym = ksym_lookup((uint64_t)caller_rip, &trace_off);
            vga_draw_string_at(4, scr_row, "[<", 8);
            vga_draw_string_at(6, scr_row, hbuf_rip, 11);
            if (trace_sym) {
                scr_trace[0] = '\0';
                panic_strlcpy(scr_trace, ">] ", sizeof(scr_trace));
                panic_strlcat(scr_trace, trace_sym->name, sizeof(scr_trace));
                vga_draw_string_at(24, scr_row, scr_trace, 8);
            } else {
                vga_draw_string_at(24, scr_row, ">] ? code+0x0", 8);
            }
            scr_row++;
        }

        uint64_t* s_rbp = (uint64_t*)rbp;
        for (int f = 0; f < 4 && s_rbp != NULL && scr_row < 16; f++) {
            if (!is_valid_stack_addr((uint64_t)s_rbp)) break;
            uint64_t next_rbp = s_rbp[0];
            uint64_t f_rip = s_rbp[1];
            if (!is_valid_code_addr(f_rip)) break;

            uint64_t trace_off = 0;
            const struct kernel_symbol* trace_sym = ksym_lookup(f_rip, &trace_off);
            if (f_rip == (uint64_t)caller_rip ||
                (trace_sym && (strcmp(trace_sym->name, "kernel_panic_extended") == 0 ||
                              strcmp(trace_sym->name, "kernel_panic_diagnostic") == 0 ||
                              strcmp(trace_sym->name, "kernel_panic") == 0))) {
                if (next_rbp <= (uint64_t)s_rbp || next_rbp - (uint64_t)s_rbp > 0x100000) break;
                s_rbp = (uint64_t*)next_rbp;
                continue;
            }

            char scr_trace[80];
            char hbuf_rip[20];
            hex_to_str(f_rip, hbuf_rip);
            vga_draw_string_at(4, scr_row, "[<", 8);
            vga_draw_string_at(6, scr_row, hbuf_rip, 11);
            if (trace_sym) {
                scr_trace[0] = '\0';
                panic_strlcpy(scr_trace, ">] ", sizeof(scr_trace));
                panic_strlcat(scr_trace, trace_sym->name, sizeof(scr_trace));
                vga_draw_string_at(24, scr_row, scr_trace, 8);
            } else {
                vga_draw_string_at(24, scr_row, ">] ? code+0x0", 8);
            }
            scr_row++;

            if (next_rbp <= (uint64_t)s_rbp || next_rbp - (uint64_t)s_rbp > 0x100000) break;
            s_rbp = (uint64_t*)next_rbp;
        }

        vga_draw_string_at(2, 17, "---[ SYSTEM HALTED: Power off or reset hardware ]---", 12);
        vga_draw_string_at(2, 18, "Press [ESC] to reboot.", 14);
    } else {
        // GPU driver was brutally disabled -> direct emergency write to text mode memory!
        vga_emergency_text_write(0, 0,  "================================================================================", 0x4F);
        vga_emergency_text_write(20, 1, " *** KERNEL PANIC: NOT SYNCING *** ", 0x4F);
        vga_emergency_text_write(0, 2,  "================================================================================", 0x4F);

        char em_diag[80];
        em_diag[0] = '\0';
        panic_strlcpy(em_diag, "  --> ", sizeof(em_diag));
        panic_strlcat(em_diag, diag_loc, sizeof(em_diag));
        panic_strlcat(em_diag, " in '", sizeof(em_diag));
        panic_strlcat(em_diag, func, sizeof(em_diag));
        panic_strlcat(em_diag, "':", sizeof(em_diag));
        vga_emergency_text_write(0, 4, em_diag, 0x4F);

        int em_row = 5;
        if (reason && strlen(reason) > 70) {
            size_t split = 70;
            for (size_t i = 70; i > 20; i--) {
                if (reason[i] == ' ') { split = i; break; }
            }
            char em1[80];
            panic_strlcpy(em1, "      ", sizeof(em1));
            size_t c1 = split < 70 ? split : 69;
            memcpy(em1 + 6, reason, c1);
            em1[6 + c1] = '\0';
            vga_emergency_text_write(0, em_row++, em1, 0x4F);

            const char* rem = reason + split;
            while (*rem == ' ') rem++;
            char em2[80];
            panic_strlcpy(em2, "      ", sizeof(em2));
            panic_strlcat(em2, rem, sizeof(em2));
            vga_emergency_text_write(0, em_row++, em2, 0x4F);
        } else {
            char em_prob[80];
            em_prob[0] = '\0';
            panic_strlcpy(em_prob, "      ", sizeof(em_prob));
            panic_strlcat(em_prob, reason ? reason : "Hardware Display Fault", sizeof(em_prob));
            vga_emergency_text_write(0, em_row++, em_prob, 0x4F);
        }

        em_row++;
        char em_loc[80];
        char rip_hex[20];
        hex_to_str((uint64_t)rip, rip_hex);
        em_loc[0] = '\0';
        panic_strlcpy(em_loc, "  Location: ", sizeof(em_loc));
        panic_strlcat(em_loc, file, sizeof(em_loc));
        if (line > 0) {
            panic_strlcat(em_loc, ":", sizeof(em_loc));
            panic_strlcat(em_loc, line_str, sizeof(em_loc));
        }
        panic_strlcat(em_loc, " | RIP: ", sizeof(em_loc));
        panic_strlcat(em_loc, rip_hex, sizeof(em_loc));
        vga_emergency_text_write(0, em_row++, em_loc, 0x4F);

        em_row++;
        vga_emergency_text_write(2, em_row++, "Display pipeline is unrecoverable. Hardware halted.", 0x4F);
        vga_emergency_text_write(2, em_row++, "Complete register and stack trace was dispatched to Serial COM1 (0x3F8).", 0x4F);

        vga_emergency_text_write(2, 21, "---[ SYSTEM HALTED: Power off or reset hardware ]---", 0x4F);
        vga_emergency_text_write(2, 22, "Press [ESC] to reboot.", 0x4E);
        vga_emergency_text_write(0, 24, "======================== SYSTEM HALTED: PLEASE RESET ===========================", 0x4F);
    }

    // 4. Disable auxiliary device (mouse) so mouse movements cannot interfere
    outb(0x64, 0xA7); // Disable PS/2 mouse interface
    outb(0x64, 0xAE); // Enable PS/2 keyboard interface

    // Drain all stale bytes in the 8042 controller buffers
    for (int drain = 0; drain < 64; drain++) {
        if (inb(0x64) & 0x01) {
            inb(0x60);
        }
        io_wait();
    }

    // Interactive halt loop: Wait STRICTLY for physical keyboard ESC key (scancode 0x01)
    while (1) {
#if defined(__riscv)
        if (serial_has_char()) {
            char c = serial_getchar();
            if (c == 27 || c == 'r' || c == 'R') {
                serial_puts("[+] Operator requested system reboot via serial [ESC]. Resetting hardware...\n");
                panic_reboot();
            }
        }
#else
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            uint8_t sc = inb(0x60);
            // Bit 5 (0x20) in status register is 1 for aux/mouse, 0 for keyboard!
            // Only trigger if data is from keyboard (!(status & 0x20)) and sc is ESC (0x01)
            if (!(status & 0x20) && sc == 0x01) {
                serial_puts("[+] Operator requested system reboot via [ESC]. Resetting hardware...\n");
                panic_reboot();
            }
        }
#endif
        for (volatile int i = 0; i < 50000; i++) {
            io_wait();
        }
    }
}

void kernel_panic_diagnostic(const char* file, int line, const char* func, const char* reason) {
    void* rip = __builtin_return_address(0);
    kernel_panic_extended(file, line, func, reason, NULL, rip);
}

void kernel_panic(const char* reason) {
    void* rip = __builtin_return_address(0);
    kernel_panic_extended(NULL, 0, NULL, reason, NULL, rip);
}

void kernel_panic_exception(uint8_t vector, uint64_t err_code, struct interrupt_frame* frame) {
#if defined(__riscv)
    __asm__ volatile ("csrci sstatus, 2");
#else
    __asm__ volatile ("cli");
#endif
    s_panic_in_progress = 1;

    uint64_t offset = 0;
    const struct kernel_symbol* sym = (frame != NULL) ? ksym_lookup(frame->rip, &offset) : NULL;

    const char* file = NULL;
    int line = 0;
    const char* func = NULL;

    if (sym != NULL) {
        file = sym->file;
        line = (int)sym->line;
        func = sym->name;
    } else {
        if (frame != NULL && (frame->cs & 3) == 3) {
            file = "userspace";
            line = 0;
            func = "ring3_task";
        } else {
            file = "<kernel>";
            line = 0;
            func = "<unknown>";
        }
    }

    char reason_buf[256];
    reason_buf[0] = '\0';

    if (vector == 14) {
        // Page Fault (#PF)
        uint64_t cr2 = 0;
#if defined(__riscv)
        __asm__ volatile ("csrr %0, stval" : "=r"(cr2));
#else
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
#endif
        char cr2_hex[20];
        hex_to_str(cr2, cr2_hex);
        char err_hex[12];
        hex_to_str(err_code, err_hex);

        panic_strlcpy(reason_buf, "page fault: invalid memory access at linear address ", sizeof(reason_buf));
        panic_strlcat(reason_buf, cr2_hex, sizeof(reason_buf));
        panic_strlcat(reason_buf, " (", sizeof(reason_buf));
        panic_strlcat(reason_buf, (err_code & 1) ? "protection violation" : "non-present page", sizeof(reason_buf));
        panic_strlcat(reason_buf, (err_code & 2) ? ", write" : ", read", sizeof(reason_buf));
        panic_strlcat(reason_buf, (err_code & 4) ? ", user mode" : ", supervisor mode", sizeof(reason_buf));
        if (err_code & 8) panic_strlcat(reason_buf, ", reserved bit set", sizeof(reason_buf));
        if (err_code & 16) panic_strlcat(reason_buf, ", instruction fetch", sizeof(reason_buf));
        panic_strlcat(reason_buf, ")", sizeof(reason_buf));
    } else if (vector == 13) {
        char err_hex[12];
        hex_to_str(err_code, err_hex);
        panic_strlcpy(reason_buf, "general protection fault: privilege or segment violation (code ", sizeof(reason_buf));
        panic_strlcat(reason_buf, err_hex, sizeof(reason_buf));
        panic_strlcat(reason_buf, ")", sizeof(reason_buf));
    } else if (vector == 0) {
        panic_strlcpy(reason_buf, "division by zero: arithmetic division by zero encountered", sizeof(reason_buf));
    } else if (vector == 6) {
        panic_strlcpy(reason_buf, "invalid opcode: instruction at RIP could not be decoded", sizeof(reason_buf));
    } else if (vector == 8) {
        char err_hex[12];
        hex_to_str(err_code, err_hex);
        panic_strlcpy(reason_buf, "double fault: exception occurred while handling previous fault (code ", sizeof(reason_buf));
        panic_strlcat(reason_buf, err_hex, sizeof(reason_buf));
        panic_strlcat(reason_buf, ")", sizeof(reason_buf));
    } else {
        panic_strlcpy(reason_buf, exception_name(vector), sizeof(reason_buf));
        panic_strlcat(reason_buf, ": fatal hardware fault", sizeof(reason_buf));
    }

    serial_puts("\n[!] FATAL CPU EXCEPTION VECTOR ");
    serial_print_dec(vector);
    serial_puts(" (Error Code: ");
    serial_print_hex(err_code);
    serial_puts("): ");
    serial_puts(exception_name(vector));
    serial_puts("\n");

    kernel_panic_extended(file, line, func, reason_buf, frame, (frame != NULL) ? (void*)frame->rip : NULL);
}
