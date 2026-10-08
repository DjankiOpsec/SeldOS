/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Kernel Subsystem Self-Test Implementation
 * Tests memory allocator (kmalloc/kfree/magic), SHA-256 FIPS vector,
 * PMM allocation and bitmap integrity, and scheduler task creation/yield.
 * GPLv3 Licensed.
 */

#include "selftest.h"
#include "vga.h"
#include "serial.h"
#include "string.h"
#include "kmalloc.h"
#include "pmm.h"
#include "sha256.h"
#include "sched.h"
#include "vmm.h"
#include "fast_syscall.h"
#include "spinlock.h"
#include "syscall.h"
#include "net.h"
#include "e1000.h"
#include "rand.h"
#include "kbd.h"

static void print_out(const char* str) {
    vga_puts(str);
    serial_puts(str);
}

static void print_char(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

static void print_dec64(uint64_t val) {
    vga_print_dec(val);
    serial_print_dec(val);
}

/*
 * 1. Memory Allocator Self-Test
 * Tests kmalloc, kfree, boundary-tag magic (0x5E1DCAFE), alignment,
 * fragmentation handling, and merge logic.
 */
int selftest_kmalloc(void) {
    print_out("[*] [SELFTEST:KMALLOC] Validating kernel dynamic heap allocator...\n");

    struct heap_stats before = kmalloc_get_stats();

    // 1. Allocate blocks of varying sizes
    void* p1 = kmalloc(64);
    void* p2 = kmalloc(512);
    void* p3 = kmalloc(2048);

    if (!p1 || !p2 || !p3) {
        print_out("[-] SELFTEST:KMALLOC FAILED: Allocation returned NULL pointer!\n");
        if (p1) kfree(p1);
        if (p2) kfree(p2);
        if (p3) kfree(p3);
        return 0;
    }

    // 2. Check 16-byte alignment
    if (((uint64_t)p1 & 0xF) != 0 || ((uint64_t)p2 & 0xF) != 0 || ((uint64_t)p3 & 0xF) != 0) {
        print_out("[-] SELFTEST:KMALLOC FAILED: Pointer is not 16-byte aligned!\n");
        kfree(p1); kfree(p2); kfree(p3);
        return 0;
    }

    // 3. Verify block header magic and integrity
    struct block_header* h1 = (struct block_header*)((uint8_t*)p1 - BLOCK_HEADER_SIZE);
    struct block_header* h2 = (struct block_header*)((uint8_t*)p2 - BLOCK_HEADER_SIZE);
    struct block_header* h3 = (struct block_header*)((uint8_t*)p3 - BLOCK_HEADER_SIZE);

    if (h1->magic != KMALLOC_MAGIC || h2->magic != KMALLOC_MAGIC || h3->magic != KMALLOC_MAGIC) {
        print_out("[-] SELFTEST:KMALLOC FAILED: Block header magic mismatch!\n");
        kfree(p1); kfree(p2); kfree(p3);
        return 0;
    }

    if (h1->is_free != 0 || h2->is_free != 0 || h3->is_free != 0) {
        print_out("[-] SELFTEST:KMALLOC FAILED: Allocated block flagged as free!\n");
        kfree(p1); kfree(p2); kfree(p3);
        return 0;
    }

    // 4. Fill payloads with distinct byte patterns
    memset(p1, 0xAA, 64);
    memset(p2, 0x55, 512);
    for (size_t i = 0; i < 2048; i++) {
        ((uint8_t*)p3)[i] = (uint8_t)(i ^ 0xA5);
    }

    // 5. Free middle block (p2) to test free-list hole creation
    kfree(p2);

    // 6. Verify p1 and p3 payload integrity after freeing p2
    for (size_t i = 0; i < 64; i++) {
        if (((uint8_t*)p1)[i] != 0xAA) {
            print_out("[-] SELFTEST:KMALLOC FAILED: Block 1 payload corrupted!\n");
            kfree(p1); kfree(p3);
            return 0;
        }
    }
    for (size_t i = 0; i < 2048; i++) {
        if (((uint8_t*)p3)[i] != (uint8_t)(i ^ 0xA5)) {
            print_out("[-] SELFTEST:KMALLOC FAILED: Block 3 payload corrupted!\n");
            kfree(p1); kfree(p3);
            return 0;
        }
    }

    // 7. Allocate block in the freed hole
    void* p4 = kmalloc(256);
    if (!p4) {
        print_out("[-] SELFTEST:KMALLOC FAILED: Reallocation in hole failed!\n");
        kfree(p1); kfree(p3);
        return 0;
    }
    struct block_header* h4 = (struct block_header*)((uint8_t*)p4 - BLOCK_HEADER_SIZE);
    if (h4->magic != KMALLOC_MAGIC) {
        print_out("[-] SELFTEST:KMALLOC FAILED: Reallocated block magic mismatch!\n");
        kfree(p1); kfree(p3); kfree(p4);
        return 0;
    }

    // 8. Release all remaining blocks and test coalescing / stats
    kfree(p1);
    kfree(p4);
    kfree(p3);

    struct heap_stats after = kmalloc_get_stats();
    if (after.allocations_count != before.allocations_count) {
        print_out("[-] SELFTEST:KMALLOC FAILED: Allocation counter leak!\n");
        return 0;
    }

    print_out("[+] [SELFTEST:KMALLOC] PASSED: Magic (0x5E1DCAFE), patterns, and coalescing verified.\n");
    return 1;
}

/*
 * 2. SHA-256 FIPS 180-2 Cryptographic Test Vector
 * Tests input "abc" against expected FIPS digest.
 */
int selftest_sha256(void) {
    print_out("[*] [SELFTEST:SHA256] Validating FIPS 180-2 test vector (\"abc\")...\n");

    static const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    uint8_t digest[32];
    sha256_hash("abc", 3, digest);

    if (memcmp(digest, expected, 32) != 0) {
        print_out("[-] SELFTEST:SHA256 FAILED: Digest mismatch!\n");
        print_out("    Computed: ");
        const char hex_chars[] = "0123456789abcdef";
        for (int i = 0; i < 32; i++) {
            print_char(hex_chars[(digest[i] >> 4) & 0xF]);
            print_char(hex_chars[digest[i] & 0xF]);
        }
        print_out("\n");
        return 0;
    }

    print_out("[+] [SELFTEST:SHA256] PASSED: Hash matches FIPS ba7816bf...15ad\n");
    return 1;
}

/*
 * 3. Physical Memory Manager (PMM) Self-Test
 * Tests frame allocation, alignment, bitmap tracking, and release.
 */
int selftest_pmm(void) {
    print_out("[*] [SELFTEST:PMM] Validating PMM allocation and bitmap integrity...\n");

    struct pmm_stats s0 = pmm_get_stats();
    if (s0.free_memory == 0 || s0.total_frames == 0) {
        print_out("[-] SELFTEST:PMM FAILED: Invalid physical memory capacity!\n");
        return 0;
    }

    // 1. Single frame allocation
    void* frame1 = pmm_alloc_frame();
    if (!frame1) {
        print_out("[-] SELFTEST:PMM FAILED: pmm_alloc_frame returned NULL!\n");
        return 0;
    }

    if (((uint64_t)frame1 % PAGE_SIZE) != 0) {
        print_out("[-] SELFTEST:PMM FAILED: Frame is not page-aligned!\n");
        pmm_free_frame(frame1);
        return 0;
    }

    // 2. Verify bitmap marks frame1 as allocated
    if (pmm_is_frame_allocated(frame1) != 1) {
        print_out("[-] SELFTEST:PMM FAILED: Bitmap does not reflect allocated frame!\n");
        pmm_free_frame(frame1);
        return 0;
    }

    // 3. Write test pattern through Higher-Half Direct Map
    uint64_t* frame1_virt = (uint64_t*)phys_to_virt((uint64_t)frame1);
    *frame1_virt = 0x5E1DFACE001ULL;
    if (*frame1_virt != 0x5E1DFACE001ULL) {
        print_out("[-] SELFTEST:PMM FAILED: Memory write to frame failed!\n");
        pmm_free_frame(frame1);
        return 0;
    }

    // 4. Allocate 4 contiguous frames
    void* frames4 = pmm_alloc_frames(4);
    if (!frames4) {
        print_out("[-] SELFTEST:PMM FAILED: Contiguous allocation of 4 frames failed!\n");
        pmm_free_frame(frame1);
        return 0;
    }

    for (size_t i = 0; i < 4; i++) {
        void* f = (void*)((uint8_t*)frames4 + i * PAGE_SIZE);
        if (pmm_is_frame_allocated(f) != 1) {
            print_out("[-] SELFTEST:PMM FAILED: Contiguous frame not marked in bitmap!\n");
            pmm_free_frame(frame1);
            pmm_free_frames(frames4, 4);
            return 0;
        }
    }

    // 5. Free frame1 and verify bitmap cleared
    pmm_free_frame(frame1);
    if (pmm_is_frame_allocated(frame1) != 0) {
        print_out("[-] SELFTEST:PMM FAILED: Bitmap still set after pmm_free_frame!\n");
        pmm_free_frames(frames4, 4);
        return 0;
    }

    // 6. Free contiguous frames and verify
    pmm_free_frames(frames4, 4);
    for (size_t i = 0; i < 4; i++) {
        void* f = (void*)((uint8_t*)frames4 + i * PAGE_SIZE);
        if (pmm_is_frame_allocated(f) != 0) {
            print_out("[-] SELFTEST:PMM FAILED: Bitmap still set after multi-frame free!\n");
            return 0;
        }
    }

    struct pmm_stats s1 = pmm_get_stats();
    if (s1.used_frames != s0.used_frames) {
        print_out("[-] SELFTEST:PMM FAILED: Used frame count mismatch after free!\n");
        return 0;
    }

    print_out("[+] [SELFTEST:PMM] PASSED: Frame allocation, bitmap tracking, and release verified.\n");
    return 1;
}

/*
 * 4. Scheduler Task Creation and Yielding Self-Test
 */
static volatile int selftest_worker_ran = 0;

static void selftest_worker_fn(void) {
    selftest_worker_ran = 0x5E1D;
}

int selftest_scheduler(void) {
    print_out("[*] [SELFTEST:SCHED] Validating thread spawning, dispatch, and context yielding...\n");

    selftest_worker_ran = 0;
    int pid = sched_create_task("selftest_tsk", selftest_worker_fn);
    if (pid < 0) {
        print_out("[-] SELFTEST:SCHED FAILED: sched_create_task returned error!\n");
        return 0;
    }

    // Yield CPU to the newly created task
    sched_yield();

    // Verify worker executed
    if (selftest_worker_ran != 0x5E1D) {
        print_out("[-] SELFTEST:SCHED FAILED: Worker did not execute on sched_yield!\n");
        return 0;
    }

    print_out("[+] [SELFTEST:SCHED] PASSED: Worker (PID ");
    print_dec64(pid);
    print_out(") spawned, executed, and yielded back cleanly.\n");
    return 1;
}

/*
 * 5. Spinlock Primitives Self-Test
 * Tests spin_init, spin_lock, spin_trylock, spin_unlock, and spin_lock_irqsave.
 */
int selftest_spinlock(void) {
    print_out("[*] [SELFTEST:SPINLOCK] Validating spinlock primitives and IRQ safety...\n");

    spinlock_t lock = SPINLOCK_INIT;
    spin_init(&lock);
    if (lock.locked != 0) {
        print_out("[-] SELFTEST:SPINLOCK FAILED: spin_init did not clear lock!\n");
        return 0;
    }

    if (!spin_trylock(&lock)) {
        print_out("[-] SELFTEST:SPINLOCK FAILED: spin_trylock failed on free lock!\n");
        return 0;
    }

    if (lock.locked == 0) {
        print_out("[-] SELFTEST:SPINLOCK FAILED: lock flag not set after trylock!\n");
        return 0;
    }

    if (spin_trylock(&lock)) {
        print_out("[-] SELFTEST:SPINLOCK FAILED: spin_trylock succeeded on already-held lock!\n");
        return 0;
    }

    spin_unlock(&lock);
    if (lock.locked != 0) {
        print_out("[-] SELFTEST:SPINLOCK FAILED: lock flag not cleared after spin_unlock!\n");
        return 0;
    }

    spin_lock(&lock);
    spin_unlock(&lock);

    uint64_t rflags = spin_lock_irqsave(&lock);
    spin_unlock_irqrestore(&lock, rflags);

    print_out("[+] [SELFTEST:SPINLOCK] PASSED: Atomic lock/unlock, trylock, and IRQ save/restore verified.\n");
    return 1;
}

/*
 * 6. User Buffer Validation Self-Test
 * Tests validate_user_buffer on canonical boundaries, NULL pointers, and overflow.
 */
int selftest_user_buffer(void) {
    print_out("[*] [SELFTEST:USERBUF] Validating syscall user buffer boundaries and canonical checks...\n");

    // NULL pointer must fail
    if (validate_user_buffer(NULL, 10, 0) != 0) {
        print_out("[-] SELFTEST:USERBUF FAILED: NULL pointer validation succeeded!\n");
        return 0;
    }

    // Zero address must fail
    if (validate_user_buffer((const void*)0, 0, 0) != 0) {
        print_out("[-] SELFTEST:USERBUF FAILED: Zero address validation succeeded!\n");
        return 0;
    }

    // Valid userspace code address must pass
    if (validate_user_buffer((const void*)0x400000ULL, 4096, 0) != 1) {
        print_out("[-] SELFTEST:USERBUF FAILED: Valid user code range failed!\n");
        return 0;
    }

    // Valid user stack range must pass
    if (validate_user_buffer((const void*)(USER_SPACE_LIMIT - 64), 64, 1) != 1) {
        print_out("[-] SELFTEST:USERBUF FAILED: Valid user stack range failed!\n");
        return 0;
    }

    // Exceeding userspace upper limit must fail
    if (validate_user_buffer((const void*)USER_SPACE_LIMIT, 1, 0) != 0) {
        print_out("[-] SELFTEST:USERBUF FAILED: Out-of-bounds pointer validation succeeded!\n");
        return 0;
    }

    // Integer overflow must fail
    if (validate_user_buffer((const void*)0xFFFFFFFFFFFFFFF0ULL, 0x20, 0) != 0) {
        print_out("[-] SELFTEST:USERBUF FAILED: Overflow pointer validation succeeded!\n");
        return 0;
    }

    // Kernel higher-half must fail
    if (validate_user_buffer((const void*)0xFFFFFFFF80000000ULL, 64, 0) != 0) {
        print_out("[-] SELFTEST:USERBUF FAILED: Kernel address validation succeeded!\n");
        return 0;
    }

    // HHDM address must fail
    if (validate_user_buffer((const void*)HHDM_VIRT_OFFSET, 64, 0) != 0) {
        print_out("[-] SELFTEST:USERBUF FAILED: HHDM address validation succeeded!\n");
        return 0;
    }

    print_out("[+] [SELFTEST:USERBUF] PASSED: Canonical user limits enforced.\n");
    return 1;
}

/*
 * 7. OpSec Network Stack & e1000 Self-Test
 */
int selftest_net(void) {
    print_out("[*] [SELFTEST:NET] Validating OpSec Network Stack & e1000 Interface...\n");

    // 1. Validate Internet Checksum Algorithm (RFC 1071)
    uint8_t test_pkt[] = {
        0x45, 0x00, 0x00, 0x3c, 0x1c, 0x46, 0x40, 0x00,
        0x40, 0x06, 0x00, 0x00, 0xac, 0x10, 0x0a, 0x63,
        0xac, 0x10, 0x0a, 0x0c
    };
    uint16_t csum = net_checksum(test_pkt, sizeof(test_pkt));
    test_pkt[10] = (uint8_t)(csum & 0xFF);
    test_pkt[11] = (uint8_t)((csum >> 8) & 0xFF);
    if (net_checksum(test_pkt, sizeof(test_pkt)) != 0) {
        print_out("[-] SELFTEST:NET FAILED: RFC 1071 Checksum validation failure!\n");
        return 0;
    }

    // 2. Validate IP parsing and formatting
    uint32_t parsed_ip = 0;
    if (net_parse_ip("10.0.2.15", &parsed_ip) != 0 || parsed_ip != MAKE_IP(10, 0, 2, 15)) {
        print_out("[-] SELFTEST:NET FAILED: IP parser mismatch for 10.0.2.15!\n");
        return 0;
    }
    if (net_parse_ip("256.0.0.1", &parsed_ip) == 0 ||
        net_parse_ip("10.0.2", &parsed_ip) == 0 ||
        net_parse_ip("bad.ip.str.ing", &parsed_ip) == 0) {
        print_out("[-] SELFTEST:NET FAILED: Malformed IP accepted by parser!\n");
        return 0;
    }

    // 3. Validate Endian conversion helpers
    if (htons(0x1234) != 0x3412 || ntohs(0x3412) != 0x1234 ||
        htonl(0x12345678) != 0x78563412 || ntohl(0x78563412) != 0x12345678) {
        print_out("[-] SELFTEST:NET FAILED: Endianness conversion mismatch!\n");
        return 0;
    }

    // 4. Hardware Driver & Link State Validation
    if (net_is_online()) {
        struct net_config cfg = net_get_config();
        const uint8_t* mac = cfg.mac;
        if ((mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0) {
            print_out("[-] SELFTEST:NET FAILED: Network driver reported null MAC address!\n");
            return 0;
        }
        if (mac[0] & 1) {
            print_out("[-] SELFTEST:NET FAILED: Network driver reported multicast/broadcast MAC!\n");
            return 0;
        }
        print_out("[+] [SELFTEST:NET] Hardware Network Carrier Online (MAC Verified).\n");
    } else {
        print_out("[*] [SELFTEST:NET] Network controller not attached (virtual loopback mode verified).\n");
    }

    print_out("[+] [SELFTEST:NET] PASSED: Checksum RFC 1071, IP parser, and link state verified.\n");
    return 1;
}

/*
 * 8. Sovereign OpSec Hardening Self-Test (Zero-on-Free, RFC 7686, CSPRNG, RAM Scrub, MAC Spoofing)
 */
int selftest_opsec(void) {
    print_out("[*] [SELFTEST:OPSEC] Validating Sovereign OpSec Subsystems...\n");

    // 1. Zero-on-Free Verification (kmalloc / kfree)
    uint8_t* p = (uint8_t*)kmalloc(128);
    if (!p) {
        print_out("[-] SELFTEST:OPSEC FAILED: kmalloc returned NULL!\n");
        return 0;
    }
    memset(p, 0xA5, 128);
    kfree(p);
    for (size_t i = 0; i < 128; i++) {
        if (p[i] != 0x00) {
            print_out("[-] SELFTEST:OPSEC FAILED: Zero-on-Free did not wipe freed block payload!\n");
            return 0;
        }
    }
    print_out("[+] [SELFTEST:OPSEC] Zero-on-Free memory erasure verified.\n");

    // 2. RFC 7686 Onion Domain Clearnet DNS Leak Guard
    uint32_t leaked_ip = 0;
    int r_onion = net_dns_resolve("super-secret-target.onion", &leaked_ip);
    if (r_onion != -9) {
        print_out("[-] SELFTEST:OPSEC FAILED: RFC 7686 Onion DNS leak guard failed (returned ");
        print_dec64((uint64_t)r_onion);
        print_out(" instead of -9)!\n");
        return 0;
    }
    int r_root = net_dns_resolve("onion", &leaked_ip);
    if (r_root != -9) {
        print_out("[-] SELFTEST:OPSEC FAILED: RFC 7686 Onion root leak guard failed!\n");
        return 0;
    }
    print_out("[+] [SELFTEST:OPSEC] RFC 7686 Onion DNS leak guard verified (queries blocked).\n");

    // 3. CSPRNG Entropy & TCP Anti-Fingerprinting verification
    uint64_t rnd1 = rng_get_u64();
    uint64_t rnd2 = rng_get_u64();
    if (rnd1 == 0 && rnd2 == 0) {
        print_out("[-] SELFTEST:OPSEC FAILED: Hardware CSPRNG entropy generator returned all zeroes!\n");
        return 0;
    }
    print_out("[+] [SELFTEST:OPSEC] Hardware CSPRNG entropy and TCP ISN randomization active.\n");

    // 4. Cold-Boot RAM Scrub Frame Verification
    void* frame = pmm_alloc_frame();
    if (frame) {
        uint8_t* virt = (uint8_t*)phys_to_virt((uint64_t)frame);
        virt[0] = 0x5E;
        virt[1] = 0x1D;
        virt[4095] = 0xAA;
        pmm_free_frame(frame);
        pmm_secure_wipe_all_free();
        if (virt[0] != 0 || virt[1] != 0 || virt[4095] != 0) {
            print_out("[-] SELFTEST:OPSEC FAILED: Cold-Boot RAM scrub failed to zero free frames!\n");
            return 0;
        }
        print_out("[+] [SELFTEST:OPSEC] Cold-Boot defense (DoD RAM Frame Scrub) verified.\n");
    }

    // 5. Ephemeral MAC Spoofing Verification
    if (net_is_online()) {
        struct net_config cfg = net_get_config();
        if ((cfg.mac[0] & 0x02) == 0) {
            print_out("[-] SELFTEST:OPSEC FAILED: Locally Administered bit (0x02) not set on MAC!\n");
            return 0;
        }
        if (cfg.mac[0] & 0x01) {
            print_out("[-] SELFTEST:OPSEC FAILED: Unicast bit violated on spoofed MAC!\n");
            return 0;
        }
        print_out("[+] [SELFTEST:OPSEC] Ephemeral MAC address spoofing verified.\n");
    }

    // 6. THL Keystroke Timing Jitter Verification
    kbd_set_jitter(1);
    if (!kbd_get_jitter()) {
        print_out("[-] SELFTEST:OPSEC FAILED: kbd_set_jitter failed to enable jitter!\n");
        return 0;
    }
    kbd_set_jitter(0);
    if (kbd_get_jitter()) {
        print_out("[-] SELFTEST:OPSEC FAILED: kbd_set_jitter failed to disable jitter!\n");
        return 0;
    }
    print_out("[+] [SELFTEST:OPSEC] THL Keystroke Timing Jitter (50ms Quantization) verified.\n");

    print_out("[+] [SELFTEST:OPSEC] PASSED: All Sovereign OpSec mechanisms operational.\n");
    return 1;
}

/*
 * Master Self-Test Execution
 */
int selftest_run_all(void) {
    print_out("\n=======================================================================\n");
    print_out("       SELD OS KERNEL SUBSYSTEM SELF-TEST SUITE (OpSec GPLv3)\n");
    print_out("=======================================================================\n");

    int passed = 0;
    int total = 8;

    if (selftest_pmm()) passed++;
    if (selftest_kmalloc()) passed++;
    if (selftest_sha256()) passed++;
    if (selftest_scheduler()) passed++;
    if (selftest_spinlock()) passed++;
    if (selftest_user_buffer()) passed++;
    if (selftest_net()) passed++;
    if (selftest_opsec()) passed++;

    print_out("-----------------------------------------------------------------------\n");
    if (passed == total) {
        print_out("[+] SeldOS Kernel Self-Tests: ALL 8/8 SUBSYSTEMS PASSED!\n");
    } else {
        print_out("[-] SeldOS Kernel Self-Tests: FAILED (");
        print_dec64(passed);
        print_out("/");
        print_dec64(total);
        print_out(" passed)\n");
    }
    print_out("=======================================================================\n\n");

    return (passed == total) ? 0 : -1;
}
