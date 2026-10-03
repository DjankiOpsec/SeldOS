#include "syscall.h"
#include "vga.h"
#include "serial.h"
#include "kbd.h"

int validate_user_buffer(const void* user_ptr, size_t size, int write) {
    (void)write;
    uint64_t uptr = (uint64_t)user_ptr;

    // Reject NULL pointer / zero address
    if (uptr == 0) {
        return 0;
    }

    // Reject integer overflow: uptr + size must not wrap around 64-bit address space
    if (uptr + (uint64_t)size < uptr) {
        return 0;
    }

    // Must be strictly within canonical userspace boundary
    if (uptr + (uint64_t)size > 0x00007FFFFFFFF000ULL) {
        return 0;
    }

    return 1;
}

void syscall_dispatch(struct interrupt_frame* frame) {
    uint64_t syscall_num = frame->rax;
    uint64_t arg1 = frame->rbx;
    uint64_t arg2 = frame->rcx;
    uint64_t arg3 = frame->rdx;

    switch (syscall_num) {
        case SYS_WRITE: {
            // arg1 = fd, arg2 = buffer, arg3 = length
            (void)arg1;
            const char* buf = (const char*)arg2;
            if (!validate_user_buffer(buf, (size_t)arg3, 0)) {
                frame->rax = (uint64_t)-1;
                break;
            }
            for (uint64_t i = 0; i < arg3; i++) {
                vga_putchar(buf[i]);
                serial_putchar(buf[i]);
            }
            frame->rax = arg3;
            break;
        }
        case SYS_READ: {
            // arg1 = fd, arg2 = buffer, arg3 = length
            (void)arg1;
            char* buf = (char*)arg2;
            if (!validate_user_buffer(buf, (size_t)arg3, 1)) {
                frame->rax = (uint64_t)-1;
                break;
            }
            for (uint64_t i = 0; i < arg3; i++) {
                buf[i] = kbd_getchar();
            }
            frame->rax = arg3;
            break;
        }
        case SYS_SELD: {
            serial_puts("[SELD-SYS] Kernel ABI handshake verification (SYS_SELD)\n");
            frame->rax = 0x5E1D; // "SELD" identification code
            break;
        }
        case SYS_EXIT: {
            vga_puts("\n[SELD-SYS] Process exited with code: ");
            vga_print_dec(arg1);
            vga_puts("\n");
            frame->rax = 0;
            break;
        }
        default:
            vga_puts("\n[SELD-SYS] Unknown syscall: ");
            vga_print_dec(syscall_num);
            vga_puts("\n");
            frame->rax = (uint64_t)-1;
            break;
    }
}
