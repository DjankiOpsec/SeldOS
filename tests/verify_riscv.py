#!/usr/bin/env python3
"""
Comprehensive Integration & Verification Suite for SeldOS on RISC-V (RV64GC).
Tests:
1. Boot to OpenSBI (S-mode) -> Kernel initialization -> Sv39 Higher-Half Paging.
2. Kernel 8/8 subsystem self-tests (PMM, KMALLOC, SHA256, SCHED, SPINLOCK, USERBUF, NET, OPSEC).
3. /bin/init (U-mode PID 1) boot checks (5/5 checks passed).
4. Launch SNL Sovereign Shell (/bin/sh).
5. Interactive shell commands:
   - 'uname -a' (architecture verification: riscv64)
   - 'cat readme.txt' (SeldFS filesystem read)
   - 'sha256sum readme.txt' (SHA-256 calculation & validation)
   - 'echo ...' (standard output)
   - 'selftest' (8/8 userspace Ring 3 / U-mode tests)
   - 'poweroff' (safe system shutdown & DoD RAM frame wipe)
"""

import subprocess
import time
import sys

def main():
    print("[*] Launching QEMU instance for SeldOS RISC-V validation...")
    cmd = [
        "qemu-system-riscv64",
        "-M", "virt",
        "-m", "128M",
        "-nographic",
        "-bios", "default",
        "-kernel", "build/kernel-riscv64.elf"
    ]

    p = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )

    # Allow kernel and init to finish booting into shell
    time.sleep(2.5)

    commands = [
        "uname -a\n",
        "cat readme.txt\n",
        "sha256sum readme.txt\n",
        "echo seldos-riscv-verified\n",
        "selftest\n",
        "poweroff\n"
    ]

    for c in commands:
        p.stdin.write(c)
        p.stdin.flush()
        time.sleep(0.4)

    try:
        out, _ = p.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()
        print("[-] Timed out waiting for QEMU!")
        print(out[-2000:])
        sys.exit(1)

    print("\n=== VERIFICATION 1: Kernel Boot & Sv39 Higher-Half Paging ===")
    assert "Hardened RISC-V 64-bit (RV64GC) Supervisor Kernel Initialized" in out, "Kernel banner not found!"
    assert "VMM: RISC-V Sv39 Higher-Half Paging active" in out, "Sv39 Paging not active!"
    print("[+] PASS: Kernel booted in S-mode with Sv39 paging.")

    print("\n=== VERIFICATION 2: Kernel Subsystem Self-Tests (8/8) ===")
    assert "SeldOS Kernel Self-Tests: ALL 8/8 SUBSYSTEMS PASSED!" in out, "Kernel self-tests failed!"
    print("[+] PASS: Kernel self-tests 8/8 passed.")

    print("\n=== VERIFICATION 3: Userspace /bin/init U-mode Self-Checks ===")
    assert "[init] All userspace initialization self-checks PASSED." in out, "Init self-checks failed!"
    print("[+] PASS: /bin/init executed and verified U-mode privilege.")

    print("\n=== VERIFICATION 4: Shell Interactive Execution & Commands ===")
    assert "riscv64" in out, "uname -a failed to report riscv64!"
    assert "b7e2f60f9a2d63440300cc0c24d00d41a19c8fad8fe518a8bf42695583a8b46c" in out, "sha256sum check failed!"
    assert "seldos-riscv-verified" in out, "echo verification failed!"
    assert "ALL 8/8 USERSPACE TESTS PASSED!" in out, "Ring 3 selftest failed!"
    print("[+] PASS: Interactive shell commands & 8/8 userspace self-tests passed.")

    print("\n=== VERIFICATION 5: Poweroff & Cold-Boot RAM Scrub ===")
    assert "RAM scrub complete" in out, "RAM scrub not completed on poweroff!"
    print("[+] PASS: Clean poweroff with Cold-Boot defense.")

    print("\n" + "=" * 55)
    print(" [SUCCESS] ALL SELDOS RISC-V VERIFICATIONS PASSED!")
    print("=" * 55 + "\n")

if __name__ == "__main__":
    main()
