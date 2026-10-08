ARCH ?= x86_64

# Common kernel C sources across all architectures
C_SRCS_COMMON = kernel/kernel.c kernel/drivers/vga.c kernel/drivers/serial.c kernel/drivers/kbd.c kernel/drivers/mouse.c kernel/drivers/audio.c kernel/drivers/ramdisk.c kernel/drivers/ata.c kernel/drivers/pit.c kernel/drivers/pci.c kernel/drivers/e1000.c kernel/drivers/pcnet.c kernel/net/net.c kernel/sys/syscall.c kernel/sys/string.c kernel/sys/fast_syscall.c kernel/sys/selftest.c kernel/sys/elf.c kernel/sys/panic.c kernel/sys/ksyms.c kernel/sys/boot_anim.c kernel/shell/seldshell.c kernel/mm/pmm.c kernel/mm/vmm.c kernel/mm/kmalloc.c kernel/fs/seldfs.c kernel/crypto/rand.c kernel/crypto/sha256.c kernel/sched/sched.c

LIBC_SRCS = userspace/libc/src/syscall.c \
            userspace/libc/src/memory.c \
            userspace/libc/src/string.c \
            userspace/libc/src/stdio.c \
            userspace/libc/src/unistd.c \
            userspace/libc/src/stdlib.c \
            userspace/libc/src/sha256.c \
            userspace/libc/src/seld_x25519.c \
            userspace/libc/src/seld_aes128_gcm.c \
            userspace/libc/src/seld_hkdf.c \
            userspace/libc/src/seld_tls.c \
            userspace/libc/src/seld_vless.c

UTILS = init sh ls cat echo rm sha256sum uname ps fm download oracle fetch reboot poweroff purge stealth

# ---------------------------------------------------------------------------
# x86_64 Architecture Definitions
# ---------------------------------------------------------------------------
X86_CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Ikernel/include -O2 -fno-omit-frame-pointer
X86_LDFLAGS = -n -T kernel/arch/x86_64/linker.ld -nostdlib -no-pie
X86_USER_CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -fno-asynchronous-unwind-tables -mno-red-zone -mcmodel=small -Wall -Wextra -Iuserspace/libc/include -O2

X86_ASM_SRCS = kernel/arch/x86_64/multiboot_header.asm kernel/arch/x86_64/boot.asm kernel/arch/x86_64/idt_asm.asm kernel/arch/x86_64/switch.asm kernel/arch/x86_64/syscall_entry.asm kernel/arch/x86_64/userspace_blob.asm kernel/arch/x86_64/logo_blob.asm
X86_ASM_OBJS = $(X86_ASM_SRCS:.asm=.o)
X86_C_OBJS = $(C_SRCS_COMMON:.c=.o) kernel/sys/idt.o kernel/sys/gdt.o
X86_OBJS = $(X86_ASM_OBJS) $(X86_C_OBJS)

X86_LIBC_OBJS = build/libc_syscall.o \
                build/libc_memory.o \
                build/libc_string.o \
                build/libc_stdio.o \
                build/libc_unistd.o \
                build/libc_stdlib.o \
                build/libc_sha256.o \
                build/libc_seld_x25519.o \
                build/libc_seld_aes128_gcm.o \
                build/libc_seld_hkdf.o \
                build/libc_seld_tls.o \
                build/libc_seld_vless.o

X86_LIBSNL = build/libsnl.a
X86_ALL_BINS = $(addprefix build/bin/, $(UTILS))

DOOM_CFLAGS = $(X86_USER_CFLAGS) -Iuserspace/doom -DNORMALUNIX -DLINUX -DSNDSERV -D_DEFAULT_SOURCE -w
DOOM_SRCS = dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c doomgeneric.c doomgeneric_seld.c
DOOM_OBJS = $(patsubst %.c, build/obj_doom_%.o, $(DOOM_SRCS))

X86_USER_BIN = build/userspace.bin
X86_USER_CRT0 = build/userspace_crt0.o

X86_KERNEL_BIN = build/kernel.bin
X86_ISO_IMAGE = build/seldos.iso
X86_DISK_IMG = build/disk.img

# ---------------------------------------------------------------------------
# RISC-V 64-bit Architecture Definitions
# ---------------------------------------------------------------------------
CC_RV ?= clang
LD_RV ?= ld.lld
AR_RV ?= llvm-ar
OBJCOPY_RV ?= llvm-objcopy

RV_TARGET = --target=riscv64-unknown-none-elf -march=rv64gc -mabi=lp64d -mcmodel=medany
RV_CFLAGS = $(RV_TARGET) -ffreestanding -fno-stack-protector -fno-pic -fno-pie -Wall -Wextra -Ikernel/include -O2 -fno-omit-frame-pointer
RV_USER_CFLAGS = $(RV_TARGET) -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-asynchronous-unwind-tables -Wall -Wextra -Iuserspace/libc/include -O2
RV_LDFLAGS = -T kernel/arch/riscv64/linker.ld -nostdlib

RV_DIR = build/rv64
RV_KOBJ = $(RV_DIR)/kobj
RV_BIN = $(RV_DIR)/bin

RV_KERNEL_ELF = build/kernel-riscv64.elf
RV_DISK_IMG = build/disk_riscv64.img
RV_USER_BIN = $(RV_DIR)/userspace.bin
RV_CRT0 = $(RV_DIR)/crt0.o
RV_LIBSNL = $(RV_DIR)/libsnl.a

RV_LIBC_OBJS = $(addprefix $(RV_DIR)/, $(notdir $(LIBC_SRCS:.c=.o)))
RV_USER_BINS = $(addprefix $(RV_BIN)/, $(UTILS))

RV_C_KOBJS = $(addprefix $(RV_KOBJ)/, $(notdir $(C_SRCS_COMMON:.c=.o))) $(RV_KOBJ)/trap.o
RV_ASM_KOBJS = $(RV_KOBJ)/boot.o $(RV_KOBJ)/switch.o $(RV_KOBJ)/syscall_entry.o $(RV_KOBJ)/trap_entry.o $(RV_KOBJ)/userspace_blob.o $(RV_KOBJ)/logo_blob.o $(RV_KOBJ)/disk_blob.o
RV_ALL_KOBJS = $(RV_C_KOBJS) $(RV_ASM_KOBJS)

# ---------------------------------------------------------------------------
# Target Dispatch
# ---------------------------------------------------------------------------
ifeq ($(filter $(ARCH),riscv riscv64),)
all: $(X86_ISO_IMAGE) $(X86_DISK_IMG) build/bin/tor build/bin/doom
else
all: $(RV_KERNEL_ELF) $(RV_DISK_IMG) $(RV_BIN)/tor
endif

# x86_64 Targets
$(X86_LIBSNL): $(X86_LIBC_OBJS)
	@mkdir -p build
	ar rcs $@ $(X86_LIBC_OBJS)

build/libc_%.o: userspace/libc/src/%.c userspace/libc/include/*.h
	@mkdir -p build
	$(CC) $(X86_USER_CFLAGS) -c $< -o $@

$(X86_USER_CRT0): userspace/crt0.asm
	@mkdir -p build
	nasm -f elf64 $< -o $@

build/obj_bin_%.o: userspace/bin/%/main.c userspace/libc/include/*.h
	@mkdir -p build
	$(CC) $(X86_USER_CFLAGS) -c $< -o $@

build/bin/%: $(X86_USER_CRT0) build/obj_bin_%.o $(X86_LIBSNL) userspace/linker.ld
	@mkdir -p build/bin
	ld -T userspace/linker.ld -nostdlib -o $@ $(X86_USER_CRT0) build/obj_bin_$*.o --whole-archive $(X86_LIBSNL) --no-whole-archive

build/obj_tor_%.o: userspace/bin/tor/%.c userspace/bin/tor/*.h userspace/libc/include/*.h
	@mkdir -p build
	$(CC) $(X86_USER_CFLAGS) -Iuserspace/bin/tor -c $< -o $@

build/bin/tor: $(X86_USER_CRT0) $(addprefix build/obj_tor_, main.o socks5.o http.o html.o) $(X86_LIBSNL) userspace/linker.ld
	@mkdir -p build/bin
	ld -T userspace/linker.ld -nostdlib -o $@ $(X86_USER_CRT0) $(addprefix build/obj_tor_, main.o socks5.o http.o html.o) --whole-archive $(X86_LIBSNL) --no-whole-archive

build/obj_doom_%.o: userspace/doom/%.c
	@mkdir -p build
	$(CC) $(DOOM_CFLAGS) -c $< -o $@

build/bin/doom: $(X86_USER_CRT0) $(DOOM_OBJS) $(X86_LIBSNL) userspace/linker.ld
	@mkdir -p build/bin
	ld -T userspace/linker.ld -nostdlib -o $@ $(X86_USER_CRT0) $(DOOM_OBJS) --whole-archive $(X86_LIBSNL) --no-whole-archive

$(X86_USER_BIN): build/bin/init
	@mkdir -p build
	objcopy -O binary $< $@

kernel/arch/x86_64/userspace_blob.o: kernel/arch/x86_64/userspace_blob.asm $(X86_USER_BIN)
	nasm -f elf64 $< -o $@

kernel/arch/x86_64/logo_blob.o: kernel/arch/x86_64/logo_blob.asm
	nasm -f elf64 $< -o $@

$(X86_KERNEL_BIN): $(X86_USER_BIN) $(X86_OBJS)
	@mkdir -p build
	ld $(X86_LDFLAGS) -o $@ $(X86_OBJS)

%.o: %.asm
	nasm -f elf64 $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/drivers/%.o: kernel/drivers/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/sys/%.o: kernel/sys/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/mm/%.o: kernel/mm/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/fs/%.o: kernel/fs/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/crypto/%.o: kernel/crypto/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/sched/%.o: kernel/sched/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/net/%.o: kernel/net/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

kernel/shell/%.o: kernel/shell/%.c
	$(CC) $(X86_CFLAGS) -c $< -o $@

GRUB_FLAGS = --locales="" --fonts="" --themes="" --compress=xz
ifneq ($(wildcard /usr/lib/grub/i386-pc),)
GRUB_FLAGS += -d /usr/lib/grub/i386-pc --install-modules="multiboot2 all_video gfxterm normal iso9660 biosdisk part_msdos"
endif

$(X86_ISO_IMAGE): $(X86_KERNEL_BIN) $(X86_DISK_IMG)
	@mkdir -p iso/boot/grub
	cp $(X86_KERNEL_BIN) iso/boot/kernel.bin
	cp $(X86_DISK_IMG) iso/boot/disk.img
	grub-mkrescue $(GRUB_FLAGS) -o $(X86_ISO_IMAGE) iso

$(X86_DISK_IMG): $(X86_ALL_BINS) scripts/mkdisk.py
	@mkdir -p build
	python3 scripts/mkdisk.py $(X86_DISK_IMG)

# ---------------------------------------------------------------------------
# RISC-V 64-bit Targets
# ---------------------------------------------------------------------------
riscv: $(RV_KERNEL_ELF) $(RV_DISK_IMG)

$(RV_LIBSNL): $(RV_LIBC_OBJS)
	@mkdir -p $(RV_DIR)
	$(AR_RV) rcs $@ $(RV_LIBC_OBJS)

$(RV_DIR)/%.o: userspace/libc/src/%.c userspace/libc/include/*.h
	@mkdir -p $(RV_DIR)
	$(CC_RV) $(RV_USER_CFLAGS) -c $< -o $@

$(RV_CRT0): userspace/crt0_riscv64.S
	@mkdir -p $(RV_DIR)
	$(CC_RV) $(RV_USER_CFLAGS) -c $< -o $@

$(RV_DIR)/obj_bin_%.o: userspace/bin/%/main.c userspace/libc/include/*.h
	@mkdir -p $(RV_DIR)
	$(CC_RV) $(RV_USER_CFLAGS) -c $< -o $@

$(RV_DIR)/obj_tor_%.o: userspace/bin/tor/%.c userspace/bin/tor/*.h userspace/libc/include/*.h
	@mkdir -p $(RV_DIR)
	$(CC_RV) $(RV_USER_CFLAGS) -Iuserspace/bin/tor -c $< -o $@

$(RV_BIN)/tor: $(RV_CRT0) $(addprefix $(RV_DIR)/obj_tor_, main.o socks5.o http.o html.o) $(RV_LIBSNL) userspace/linker_riscv64.ld
	@mkdir -p $(RV_BIN)
	$(LD_RV) -T userspace/linker_riscv64.ld -nostdlib -o $@ $(RV_CRT0) $(addprefix $(RV_DIR)/obj_tor_, main.o socks5.o http.o html.o) --whole-archive $(RV_LIBSNL) --no-whole-archive

$(RV_BIN)/%: $(RV_CRT0) $(RV_DIR)/obj_bin_%.o $(RV_LIBSNL) userspace/linker_riscv64.ld
	@mkdir -p $(RV_BIN)
	$(LD_RV) -T userspace/linker_riscv64.ld -nostdlib -o $@ $(RV_CRT0) $(RV_DIR)/obj_bin_$*.o --whole-archive $(RV_LIBSNL) --no-whole-archive

$(RV_USER_BIN): $(RV_BIN)/init
	@mkdir -p $(RV_DIR)
	$(OBJCOPY_RV) -O binary $< $@

$(RV_DISK_IMG): $(RV_USER_BINS) scripts/mkdisk.py
	@mkdir -p build
	python3 scripts/mkdisk.py $(RV_DISK_IMG) --bin-dir $(RV_BIN)

$(RV_KOBJ)/disk_blob.o: kernel/arch/riscv64/disk_blob.S $(RV_DISK_IMG)
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/userspace_blob.o: kernel/arch/riscv64/userspace_blob.S $(RV_USER_BIN)
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/logo_blob.o: kernel/arch/riscv64/logo_blob.S seldos_logo.svgz
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/boot.o: kernel/arch/riscv64/boot.S
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/switch.o: kernel/arch/riscv64/switch.S
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/syscall_entry.o: kernel/arch/riscv64/syscall_entry.S
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/trap_entry.o: kernel/arch/riscv64/trap_entry.S
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/trap.o: kernel/arch/riscv64/trap.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/drivers/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/sys/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/mm/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/fs/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/crypto/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/sched/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/net/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KOBJ)/%.o: kernel/shell/%.c
	@mkdir -p $(RV_KOBJ)
	$(CC_RV) $(RV_CFLAGS) -c $< -o $@

$(RV_KERNEL_ELF): $(RV_ALL_KOBJS) kernel/arch/riscv64/linker.ld
	@mkdir -p build
	$(LD_RV) $(RV_LDFLAGS) -o $@ $(RV_ALL_KOBJS)

# ---------------------------------------------------------------------------
# Emulation & Gateway Targets
# ---------------------------------------------------------------------------
gateway: build/bin/tor
	@python3 scripts/opsec_gateway.py --daemon

gateway-stop:
	@python3 scripts/opsec_gateway.py --stop

gateway-status:
	@python3 scripts/opsec_gateway.py --status

QEMU_AUDIO ?= -audiodev pa,id=audio0 -device AC97,audiodev=audio0 -machine pcspk-audiodev=audio0

qemu: $(X86_ISO_IMAGE)
	@python3 scripts/opsec_gateway.py --daemon 2>/dev/null || true
	qemu-system-x86_64 -cdrom $(X86_ISO_IMAGE) -serial stdio -vga std -net nic,model=e1000 -net user $(QEMU_AUDIO)

qemu-direct: $(X86_KERNEL_BIN) $(X86_DISK_IMG)
	@python3 scripts/opsec_gateway.py --daemon 2>/dev/null || true
	qemu-system-x86_64 -kernel $(X86_KERNEL_BIN) -drive file=$(X86_DISK_IMG),format=raw -serial stdio -vga std -net nic,model=e1000 -net user $(QEMU_AUDIO)

qemu-riscv: riscv
	qemu-system-riscv64 -M virt -m 128M -nographic -bios default -kernel $(RV_KERNEL_ELF)

clean:
	rm -rf $(X86_OBJS) $(X86_LIBC_OBJS) $(X86_LIBSNL) $(X86_USER_BIN) $(X86_USER_CRT0) build iso/boot/kernel.bin iso/boot/disk.img

.PHONY: all riscv qemu qemu-direct qemu-riscv clean gateway gateway-stop gateway-status
