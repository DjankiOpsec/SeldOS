#include "idt.h"
#include "vga.h"
#include "serial.h"
#include "io.h"
#include "kbd.h"
#include "mouse.h"
#include "pit.h"
#include "sched.h"

static struct idt_entry idt[256];
static struct idt_ptr idtp;

// Declared in idt_asm.asm
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);
extern void isr128(void);

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = (uint16_t)(base & 0xFFFF);
    idt[num].selector    = sel;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].zero        = 0;
}

static void remap_pic(void) {
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    outb(0x21, 0x20); // Master PIC vector offset 32 (0x20)
    io_wait();
    outb(0xA1, 0x28); // Slave PIC vector offset 40 (0x28)
    io_wait();

    outb(0x21, 0x04); // Master has slave at IRQ2
    io_wait();
    outb(0xA1, 0x02); // Slave identity
    io_wait();

    outb(0x21, 0x01); // 8086 mode
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    // Mask interrupts except timer (0), keyboard (1), and cascade (2) on master,
    // and mouse (4 / IRQ12) on slave
    outb(0x21, 0xF8);
    outb(0xA1, 0xEF);
}

static irq_handler_t s_irq_handlers[16] = {0};

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        s_irq_handlers[irq] = handler;
    }
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

extern void syscall_dispatch(struct interrupt_frame* frame);

void isr_handler(struct interrupt_frame* frame) {
    if (frame->int_no < 32) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("\n[SELD EXCEPTION] Vector: ");
        vga_print_dec(frame->int_no);
        vga_puts(" Err: ");
        vga_print_hex(frame->err_code);
        vga_puts(" RIP: ");
        vga_print_hex(frame->rip);
        vga_puts(" RSP: ");
        vga_print_hex(frame->rsp);
        vga_puts("\n  RDI: ");
        vga_print_hex(frame->rdi);
        vga_puts(" RSI: ");
        vga_print_hex(frame->rsi);
        vga_puts(" RDX: ");
        vga_print_hex(frame->rdx);
        vga_puts(" RAX: ");
        vga_print_hex(frame->rax);
        vga_puts("\n  CS: ");
        vga_print_hex(frame->cs);
        vga_puts(" SS: ");
        vga_print_hex(frame->ss);
        vga_puts(" SeldOS halts!\n");

        serial_puts("\n[SELD EXCEPTION] Vector: ");
        serial_print_dec(frame->int_no);
        serial_puts(" Err: ");
        serial_print_hex(frame->err_code);
        serial_puts(" RIP: ");
        serial_print_hex(frame->rip);
        serial_puts(" RSP: ");
        serial_print_hex(frame->rsp);
        serial_puts("\n  RDI: ");
        serial_print_hex(frame->rdi);
        serial_puts(" RSI: ");
        serial_print_hex(frame->rsi);
        serial_puts(" RDX: ");
        serial_print_hex(frame->rdx);
        serial_puts(" RAX: ");
        serial_print_hex(frame->rax);
        serial_puts(" CS: ");
        serial_print_hex(frame->cs);
        serial_puts(" SS: ");
        serial_print_hex(frame->ss);
        serial_puts("\n");

        while (1) {
            __asm__ volatile ("hlt");
        }
    } else if (frame->int_no == 32) {
        // Timer IRQ0
        pit_handle_interrupt();
        outb(0x20, 0x20);
        sched_tick();
    } else if (frame->int_no == 33) {
        // Keyboard IRQ1
        while (inb(0x64) & 0x01) {
            uint8_t status = inb(0x64);
            uint8_t data = inb(0x60);
            if (status & 0x20) {
                mouse_handle_byte(data);
            } else {
                kbd_handle_scancode(data);
            }
        }
        outb(0x20, 0x20);
    } else if (frame->int_no == 44) {
        // Mouse IRQ12
        while (inb(0x64) & 0x01) {
            uint8_t status = inb(0x64);
            uint8_t data = inb(0x60);
            if (status & 0x20) {
                mouse_handle_byte(data);
            } else {
                kbd_handle_scancode(data);
            }
        }
        outb(0xA0, 0x20); // Slave EOI
        outb(0x20, 0x20); // Master EOI
    } else if (frame->int_no >= 32 && frame->int_no < 48) {
        uint8_t irq = (uint8_t)(frame->int_no - 32);
        if (s_irq_handlers[irq]) {
            s_irq_handlers[irq](frame);
        }
        if (frame->int_no >= 40) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    } else if (frame->int_no == 128) {
        // Syscall vector 0x80
        syscall_dispatch(frame);
    }
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    remap_pic();

    void* isr_table[] = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
        isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
        isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
    };

    for (uint8_t i = 0; i < 48; i++) {
        // Ring 0 interrupt gate (0x8E = present, ring0, 64bit interrupt gate)
        idt_set_gate(i, (uint64_t)isr_table[i], 0x08, 0x8E);
    }

    // Syscall 0x80: Ring 3 accessible (0xEE = present, ring3, 64bit interrupt gate)
    idt_set_gate(128, (uint64_t)isr128, 0x08, 0xEE);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
    __asm__ volatile ("sti");
}
