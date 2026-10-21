#include "syscall.h"
#include "fast_syscall.h"
#include "vga.h"
#include "serial.h"
#include "kbd.h"

#include "vmm.h"
#include "pmm.h"
#include "string.h"

int validate_user_buffer(const void* user_ptr, size_t size, int write) {
    uint64_t uptr = (uint64_t)user_ptr;

    // Reject NULL pointer / zero address or zero size
    if (uptr == 0 || size == 0) {
        return 0;
    }

    // Reject integer overflow: uptr + size must not wrap around 64-bit address space
    if (uptr + (uint64_t)size < uptr) {
        return 0;
    }

    // Must be strictly within canonical userspace boundary
    if (uptr + (uint64_t)size > USER_SPACE_LIMIT) {
        return 0;
    }

    // When running inside a user address space (CR3 != kernel root), verify page presence & access
    uint64_t active_cr3 = vmm_get_active_root();
    if (active_cr3 && active_cr3 != vmm_get_kernel_pml4()) {
        uint64_t* user_pml4 = (uint64_t*)phys_to_virt(active_cr3);
        uint64_t start_page = uptr & ~0xFFFULL;
        uint64_t end_page = (uptr + (uint64_t)size - 1) & ~0xFFFULL;
        for (uint64_t va = start_page; va <= end_page; va += PAGE_SIZE) {
            uint64_t pa = vmm_get_mapping(user_pml4, va);
            if (!pa) {
                // If page is within user stack boundaries, dynamically allocate and map it on demand
                if (va >= USER_STACK_BOTTOM && va < USER_STACK_PAGE) {
                    void* frame = pmm_alloc_frame();
                    if (!frame) {
                        return 0;
                    }
                    memset(phys_to_virt((uint64_t)frame), 0, PAGE_SIZE);
                    if (vmm_map_page(user_pml4, va, (uint64_t)frame,
                                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE) != 0) {
                        pmm_free_frame(frame);
                        return 0;
                    }
                    continue;
                }
                return 0;
            }

            // Verify write permission on page when write access requested
            if (!vmm_check_access(user_pml4, va, write)) {
                return 0;
            }
        }
    }

    return 1;
}

void syscall_dispatch(struct interrupt_frame* frame) {
    frame->rax = fast_syscall_dispatcher(frame->rax, frame->rbx, frame->rcx, frame->rdx, 0);
}
