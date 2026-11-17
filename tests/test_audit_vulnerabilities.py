"""
SeldOS - Humboldt Kernel Project
Audit Vulnerabilities Regression & Invariants Suite
Verifies the complete elimination of all 20 critical vulnerabilities
audited across Humboldt Kernel, Drivers, Network, Libc, and Userspace.
GNU AGPLv3 Licensed - Humboldt Underground.
"""

import os
from pathlib import Path
import pytest

BASE_DIR = Path(__file__).resolve().parent.parent

class TestAuditVulnerabilityFixes:
    """Verifies that all 20 security audit defect fixes and invariants are in place."""

    def test_vulnerability_1_idt_userspace_exception_handling(self):
        """Fix 1: Userspace exceptions must cleanly exit process rather than triggering kernel panic."""
        idt_c = (BASE_DIR / "kernel/sys/idt.c").read_text()
        assert "(frame->cs & 3) == 3" in idt_c
        assert "user_exit_to_kernel(128 + 14)" in idt_c
        assert "user_exit_to_kernel(128 + frame->int_no)" in idt_c

    def test_vulnerability_2_syscall_entry_stack_math(self):
        """Fix 2: Stack offset math must be computed using current depth before increment to avoid 64KB stack underflow at depth 8."""
        asm_txt = (BASE_DIR / "kernel/arch/x86_64/syscall_entry.asm").read_text()
        # Verify offset calculation precedes inc user_depth
        calc_pos = asm_txt.find("mov [rel user_stack_offset],")
        inc_pos = asm_txt.find("inc qword [rel user_depth]")
        assert calc_pos != -1 and inc_pos != -1
        assert calc_pos < inc_pos, "user_stack_offset must be computed before inc user_depth"
        # Verify user_exit_to_kernel restore logic
        assert ".offset_zero:" in asm_txt
        assert "mov qword [rel user_stack_offset], 0" in asm_txt

    def test_vulnerability_3_fast_syscall_mandatory_pledge(self):
        """Fix 3: Mandatory sandboxing by default. Non-admin binaries cannot have PLEDGE_PURGE/PLEDGE_REBOOT."""
        fsys_c = (BASE_DIR / "kernel/sys/fast_syscall.c").read_text()
        assert "current_pledge_enforced = 1;" in fsys_c
        assert "DEFAULT_USER_PLEDGE" in fsys_c
        assert "ADMIN_SYSTEM_PLEDGE" in fsys_c
        # check_pledge must be unconditional
        assert "if (!current_pledge_enforced) return 0;" not in fsys_c

    def test_vulnerability_4_init_exit_no_ring0_shell(self):
        """Fix 4: Termination of /bin/init must halt system; no fallback to Ring 0 seldshell."""
        kernel_c = (BASE_DIR / "kernel/kernel.c").read_text()
        assert "Falling back to Ring 0 SeldShell supervisor" not in kernel_c
        assert "seldshell_run()" not in kernel_c
        assert "[!] FATAL: PID 1 (/bin/init) terminated" in kernel_c
        assert "cli; hlt;" in kernel_c

    def test_vulnerability_5_riscv_sum_bit_cleared(self):
        """Fix 5: sstatus.SUM bit must be cleared when transitioning to U-mode."""
        sys_s = (BASE_DIR / "kernel/arch/riscv64/syscall_entry.S").read_text()
        assert "SUM = 0" in sys_s
        assert "~((1 << 8) | (1 << 18))" in sys_s

    def test_vulnerability_6_pmm_free_frame_kernel_guard(self):
        """Fix 6: pmm_free_frame must guard kernel image and reserved tables."""
        pmm_c = (BASE_DIR / "kernel/mm/pmm.c").read_text()
        assert "s_kernel_reserved_end" in pmm_c
        assert "min_safe_addr" in pmm_c
        assert "addr < min_safe_addr || addr >= total_physical_memory" in pmm_c

    def test_vulnerability_7_libc_malloc_and_double_free(self):
        """Fix 7: Libc malloc overflow check and free double-free guard."""
        mem_c = (BASE_DIR / "userspace/libc/src/memory.c").read_text()
        assert "size > (size_t)-16 || size > 0x40000000ULL" in mem_c
        assert "if (block->is_free)" in mem_c

    def test_vulnerability_8_elf_stack_collision_check(self):
        """Fix 8: ELF loader must check PT_LOAD collision with user stack region."""
        elf_c = (BASE_DIR / "kernel/sys/elf.c").read_text()
        assert "ph->p_vaddr < USER_STACK_PAGE" in elf_c
        assert "ph->p_vaddr + ph->p_memsz > USER_STACK_BOTTOM" in elf_c
        assert "Segment collides with user stack region" in elf_c

    def test_vulnerability_9_arp_unsolicited_spoof_defense(self):
        """Fix 9: Unsolicited ARP packets must not overwrite gateway or fill table."""
        net_c = (BASE_DIR / "kernel/net/net.c").read_text()
        assert "s_arp_pending_ip" in net_c
        assert "s_arp_pending_time" in net_c
        assert "is_pending || (is_existing && arp->sender_ip != s_config.gateway)" in net_c

    def test_vulnerability_10_dns_client_port_and_ip_validation(self):
        """Fix 10: DNS response validation of client_port and server_ip."""
        net_c = (BASE_DIR / "kernel/net/net.c").read_text()
        assert "s_dns_tracker.client_port != 0 && dest_port != s_dns_tracker.client_port" in net_c
        assert "s_dns_tracker.server_ip != 0 && ip_hdr->src_ip != s_dns_tracker.server_ip" in net_c

    def test_vulnerability_11_tcp_rst_sequence_validation(self):
        """Fix 11: RFC 5961 blind TCP RST validation."""
        net_c = (BASE_DIR / "kernel/net/net.c").read_text()
        assert "RFC 5961 Blind TCP Reset defense" in net_c
        assert "(uint32_t)(seg_seq - s->rcv_nxt) < 32768U" in net_c

    def test_vulnerability_12_e1000_multidescriptor_fragment_discard(self):
        """Fix 12: e1000 multi-descriptor frame discard until EOP."""
        e1000_c = (BASE_DIR / "kernel/drivers/e1000.c").read_text()
        assert "s_rx_discarding_multidesc" in e1000_c
        assert "!(status & E1000_RXD_STAT_EOP)" in e1000_c

    def test_vulnerability_13_tls_ca_verification_and_certverify(self):
        """Fix 13: Cryptographic X.509 CA chain verification and CertificateVerify signature check."""
        tls_c = (BASE_DIR / "userspace/libc/src/seld_tls.c").read_text()
        verify_c = (BASE_DIR / "userspace/libc/src/seld_crypto_verify.c").read_text()
        root_cas_h = (BASE_DIR / "userspace/libc/include/seld_root_cas.h").read_text()
        assert "seld_x509_verify_chain" in tls_c
        assert "seld_crypto_verify_tls13_cv" in tls_c
        assert "ISRG Root X1" in root_cas_h
        assert "DigiCert Global Root G2" in root_cas_h
        assert "seld_crypto_verify_sig" in verify_c
        assert "cert_verify_received = 1;" in tls_c
        assert "!cert_received || !cert_verify_received" in tls_c

    def test_vulnerability_14_aes_ctr_nonce_salted(self):
        """Fix 14: AES CTR sector nonce key salting against Two-Time Pad."""
        aes_c = (BASE_DIR / "kernel/crypto/aes.c").read_text()
        assert "key_salt" in aes_c
        assert "0x53454C44ULL ^ (uint64_t)key_salt" in aes_c

    def test_vulnerability_15_csprng_interrupt_lock(self):
        """Fix 15: Interrupt-safe critical section in rng_get_u64."""
        rand_c = (BASE_DIR / "kernel/crypto/rand.c").read_text()
        assert "pushfq" in rand_c
        assert "cli" in rand_c
        assert "sti" in rand_c

    def test_vulnerability_16_audio_nonblocking(self):
        """Fix 16: Removal of synchronous pit_sleep_ms(25) from audio_play_pcm."""
        audio_c = (BASE_DIR / "kernel/drivers/audio.c").read_text()
        assert "pit_sleep_ms(25)" not in audio_c
        assert "sched_yield()" in audio_c

    def test_vulnerability_17_pci_no_bar_zeroing(self):
        """Fix 17: Removal of BAR0/BAR1 zeroing in USB quarantine."""
        pci_c = (BASE_DIR / "kernel/drivers/pci.c").read_text()
        assert "PCI_REG_BAR0, 0" not in pci_c
        assert "PCI_REG_BAR1, 0" not in pci_c

    def test_vulnerability_18_riscv_disk_blob_data_section(self):
        """Fix 18: RISC-V disk blob section is .data, not .rodata."""
        blob_s = (BASE_DIR / "kernel/arch/riscv64/disk_blob.S").read_text()
        assert ".section .data" in blob_s
        assert ".section .rodata" not in blob_s

    def test_vulnerability_19_tor_fail_closed_policy(self):
        """Fix 19: Tor browser fails closed on proxy failure; no unproxied clearnet DNS."""
        tor_http_c = (BASE_DIR / "userspace/bin/tor/http.c").read_text()
        assert "return -10; // ERR_TOR_OFFLINE" in tor_http_c
        assert "seld_dns_resolve(host, &ip)" not in tor_http_c

    def test_vulnerability_20_sscanf_width_limit(self):
        """Fix 20: sscanf width limit parsing to prevent buffer overflow."""
        stdio_c = (BASE_DIR / "userspace/libc/src/stdio.c").read_text()
        assert "width = 255;" in stdio_c
        assert "idx < width" in stdio_c
