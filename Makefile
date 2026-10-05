CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Ikernel/include -O2 -fno-omit-frame-pointer
LDFLAGS = -n -T kernel/arch/x86_64/linker.ld -nostdlib -no-pie

USER_CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -fno-asynchronous-unwind-tables -mno-red-zone -mcmodel=small -Wall -Wextra -Iuserspace/libc/include -O2

ASM_SRCS = kernel/arch/x86_64/multiboot_header.asm kernel/arch/x86_64/boot.asm kernel/arch/x86_64/idt_asm.asm kernel/arch/x86_64/switch.asm kernel/arch/x86_64/syscall_entry.asm kernel/arch/x86_64/userspace_blob.asm kernel/arch/x86_64/logo_blob.asm
C_SRCS = kernel/kernel.c kernel/drivers/vga.c kernel/drivers/serial.c kernel/drivers/kbd.c kernel/drivers/mouse.c kernel/drivers/audio.c kernel/drivers/ramdisk.c kernel/drivers/ata.c kernel/drivers/pit.c kernel/drivers/pci.c kernel/drivers/e1000.c kernel/drivers/pcnet.c kernel/net/net.c kernel/sys/idt.c kernel/sys/syscall.c kernel/sys/string.c kernel/sys/gdt.c kernel/sys/fast_syscall.c kernel/sys/selftest.c kernel/sys/elf.c kernel/sys/panic.c kernel/sys/ksyms.c kernel/sys/boot_anim.c kernel/shell/seldshell.c kernel/mm/pmm.c kernel/mm/vmm.c kernel/mm/kmalloc.c kernel/fs/seldfs.c kernel/crypto/rand.c kernel/crypto/sha256.c kernel/sched/sched.c

ASM_OBJS = $(ASM_SRCS:.asm=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# Libc sources & objects
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
            userspace/libc/src/seld_tls.c

LIBC_OBJS = build/libc_syscall.o \
            build/libc_memory.o \
            build/libc_string.o \
            build/libc_stdio.o \
            build/libc_unistd.o \
            build/libc_stdlib.o \
            build/libc_sha256.o \
            build/libc_seld_x25519.o \
            build/libc_seld_aes128_gcm.o \
            build/libc_seld_hkdf.o \
            build/libc_seld_tls.o

LIBSNL = build/libsnl.a

UTILS = init sh ls cat echo rm sha256sum uname ps fm download tor
ALL_BINS = $(addprefix build/bin/, $(UTILS))

DOOM_CFLAGS = $(USER_CFLAGS) -Iuserspace/doom -DNORMALUNIX -DLINUX -DSNDSERV -D_DEFAULT_SOURCE -w
DOOM_SRCS = dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c doomgeneric.c doomgeneric_seld.c
DOOM_OBJS = $(patsubst %.c, build/obj_doom_%.o, $(DOOM_SRCS))

USER_BIN = build/userspace.bin
USER_CRT0 = build/userspace_crt0.o

KERNEL_BIN = build/kernel.bin
ISO_IMAGE = build/seldos.iso
DISK_IMG = build/disk.img

all: $(ISO_IMAGE) $(DISK_IMG) $(TOR_BIN)

# Libc compilation and archiving
$(LIBSNL): $(LIBC_OBJS)
	@mkdir -p build
	ar rcs $@ $(LIBC_OBJS)

build/libc_%.o: userspace/libc/src/%.c userspace/libc/include/*.h
	@mkdir -p build
	$(CC) $(USER_CFLAGS) -c $< -o $@

# Userspace CRT0
$(USER_CRT0): userspace/crt0.asm
	@mkdir -p build
	nasm -f elf64 $< -o $@

# Userspace utility compilation
build/obj_bin_%.o: userspace/bin/%/main.c userspace/libc/include/*.h
	@mkdir -p build
	$(CC) $(USER_CFLAGS) -c $< -o $@

# Utility linking (standard ELF-64 executables)
build/bin/%: $(USER_CRT0) build/obj_bin_%.o $(LIBSNL) userspace/linker.ld
	@mkdir -p build/bin
	ld -T userspace/linker.ld -nostdlib -o $@ $(USER_CRT0) build/obj_bin_$*.o --whole-archive $(LIBSNL) --no-whole-archive

TOR_BIN = build/bin/tor
TOR_SRCS = userspace/bin/tor/main.c userspace/bin/tor/socks5.c userspace/bin/tor/http.c userspace/bin/tor/html.c
TOR_OBJS = $(patsubst userspace/bin/tor/%.c, build/obj_tor_%.o, $(TOR_SRCS))

build/obj_tor_%.o: userspace/bin/tor/%.c userspace/bin/tor/*.h userspace/libc/include/*.h
	@mkdir -p build
	$(CC) $(USER_CFLAGS) -Iuserspace/bin/tor -c $< -o $@

build/bin/tor: $(USER_CRT0) $(TOR_OBJS) $(LIBSNL) userspace/linker.ld
	@mkdir -p build/bin
	ld -T userspace/linker.ld -nostdlib -o $@ $(USER_CRT0) $(TOR_OBJS) --whole-archive $(LIBSNL) --no-whole-archive

# Doom compilation and linking
build/obj_doom_%.o: userspace/doom/%.c
	@mkdir -p build
	$(CC) $(DOOM_CFLAGS) -c $< -o $@

build/bin/doom: $(USER_CRT0) $(DOOM_OBJS) $(LIBSNL) userspace/linker.ld
	@mkdir -p build/bin
	ld -T userspace/linker.ld -nostdlib -o $@ $(USER_CRT0) $(DOOM_OBJS) --whole-archive $(LIBSNL) --no-whole-archive


# Embedded userspace flat binary for kernel fallback (from /bin/init)
$(USER_BIN): build/bin/init
	@mkdir -p build
	objcopy -O binary $< $@

# Kernel compilation
kernel/arch/x86_64/userspace_blob.o: kernel/arch/x86_64/userspace_blob.asm $(USER_BIN)
	nasm -f elf64 $< -o $@

kernel/arch/x86_64/logo_blob.o: kernel/arch/x86_64/logo_blob.asm
	nasm -f elf64 $< -o $@

$(KERNEL_BIN): $(USER_BIN) $(OBJS)
	@mkdir -p build
	ld $(LDFLAGS) -o $@ $(OBJS)

%.o: %.asm
	nasm -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(ISO_IMAGE): $(KERNEL_BIN) $(DISK_IMG)
	@mkdir -p iso/boot/grub
	cp $(KERNEL_BIN) iso/boot/kernel.bin
	cp $(DISK_IMG) iso/boot/disk.img
	grub-mkrescue -o $(ISO_IMAGE) iso

$(DISK_IMG): $(ALL_BINS) scripts/mkdisk.py
	@mkdir -p build
	python3 scripts/mkdisk.py $(DISK_IMG)

gateway: $(TOR_BIN)
	@python3 scripts/opsec_gateway.py --daemon

gateway-stop:
	@python3 scripts/opsec_gateway.py --stop

gateway-status:
	@python3 scripts/opsec_gateway.py --status

qemu: $(ISO_IMAGE)
	qemu-system-x86_64 -cdrom $(ISO_IMAGE) -serial stdio -vga std -net nic,model=e1000 -net user

qemu-direct: $(KERNEL_BIN) $(DISK_IMG)
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -drive file=$(DISK_IMG),format=raw -serial stdio -vga std -net nic,model=e1000 -net user

clean:
	rm -rf $(OBJS) $(LIBC_OBJS) $(LIBSNL) $(USER_BIN) $(USER_CRT0) build iso/boot/kernel.bin iso/boot/disk.img

.PHONY: all qemu qemu-direct clean
