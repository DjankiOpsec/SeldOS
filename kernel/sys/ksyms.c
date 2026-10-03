/*
 * SeldOS - Humboldt Kernel Project
 * Kernel Symbol Table Implementation for Panic & Diagnostics
 */

#include "ksyms.h"

extern void seldfs_init(void);
extern void vga_get_fb_info(void);
extern void vmm_get_mapping(void);
extern void pit_handle_interrupt(void);
extern void sched_sleep(void);
extern void seldfs_get_bitmap_stats(void);
extern void pit_get_ticks(void);
extern void cpu_driver_disable(void);
extern void vga_set_color(void);
extern void e1000_send_packet(void);
extern void vga_puts(void);
extern void kbd_init(void);
extern void sched_tick(void);
extern void vmm_destroy_address_space(void);
extern void ata_soft_reset(void);
extern void memmove(void);
extern void vga_draw_string_at(void);
extern void syscall_init_fast(void);
extern void pcnet_send_packet(void);
extern void sched_exit(void);
extern void boot_anim_step(void);
extern void pci_enable_bus_mastering(void);
extern void rng_init(void);
extern void audio_play_tone(void);
extern void net_get_arp_table(void);
extern void net_set_config(void);
extern void seldfs_format(void);
extern void jump_to_userspace(void);
extern void vga_get_humboldt_palette(void);
extern void sha256_init(void);
extern void audio_init(void);
extern void serial_has_char(void);
extern void selftest_net(void);
extern void vga_is_gpu_enabled(void);
extern void e1000_poll_packet(void);
extern void net_arp_lookup(void);
extern void vga_set_humboldt_color(void);
extern void vmm_init(void);
extern void pci_write_dword(void);
extern void memcpy(void);
extern void pmm_alloc_frame(void);
extern void mouse_poll_event(void);
extern void kernel_panic_in_progress(void);
extern void kfree(void);
extern void ata_write_sectors(void);
extern void vga_fill_rect(void);
extern void seldfs_verify_file(void);
extern void selftest_user_buffer(void);
extern void e1000_is_active(void);
extern void kbd_poll_event(void);
extern void kzalloc(void);
extern void seldfs_list_files(void);
extern void cpu_is_driver_enabled(void);
extern void boot_anim_is_active(void);
extern void net_parse_ip(void);
extern void rng_get_u64(void);
extern void audio_play_pcm(void);
extern void net_tcp_socket_close(void);
extern void net_tcp_socket_connect(void);
extern void seldfs_read_file(void);
extern void gdt_init(void);
extern void pci_read_dword(void);
extern void ata_init(void);
extern void seldfs_get_inode_by_index(void);
extern void vmm_switch_pml4(void);
extern void vga_set_console_rows(void);
extern void serial_print_dec(void);
extern void vga_init(void);
extern void selftest_spinlock(void);
extern void selftest_run_all(void);
extern void seldfs_write_file(void);
extern void seldfs_read_file_offset(void);
extern void pci_read_byte(void);
extern void vga_draw_bitmap(void);
extern void selftest_sha256(void);
extern void sched_create_task(void);
extern void mouse_get_state(void);
extern void net_arp_request(void);
extern void pci_write_byte(void);
extern void pit_init(void);
extern void net_checksum(void);
extern void pmm_is_frame_allocated(void);
extern void idt_init(void);
extern void kernel_main(void);
extern void rng_has_rdrand(void);
extern void vga_emergency_text_write(void);
extern void kbd_handle_scancode(void);
extern void serial_putchar(void);
extern void kbd_getchar(void);
extern void vga_init_fb(void);
extern void boot_anim_finish(void);
extern void pci_read_word(void);
extern void pci_write_word(void);
extern void pcnet_init(void);
extern void net_tcp_socket_is_connected(void);
extern void idt_set_gate(void);
extern void vga_print_dec(void);
extern void net_dns_resolve(void);
extern void pcnet_is_active(void);
extern void pic_unmask_irq(void);
extern void pmm_free_frames(void);
extern void pci_init(void);
extern void ramdisk_get_info(void);
extern void syscall_dispatch(void);
extern void pcnet_poll_packet(void);
extern void net_ping(void);
extern void vmm_get_kernel_pml4(void);
extern void strncmp(void);
extern void net_init(void);
extern void seldshell_init(void);
extern void serial_print_hex(void);
extern void net_tcp_socket_send(void);
extern void ramdisk_read_sectors(void);
extern void kmalloc_init(void);
extern void ata_read_sectors(void);
extern void elf_load_and_run(void);
extern void pmm_free_frame(void);
extern void pci_find_device(void);
extern void seldfs_delete_file(void);
extern void memcmp(void);
extern void vga_set_scroll_window(void);
extern void mouse_handle_irq(void);
extern void pcnet_get_mac(void);
extern void strtoull(void);
extern void sched_get_tasks(void);
extern void memset(void);
extern void seldfs_get_file_count(void);
extern void kernel_panic_exception(void);
extern void ramdisk_write_sectors(void);
extern void kbd_has_char(void);
extern void vmm_unmap_page(void);
extern void net_format_ip(void);
extern void seldfs_get_all_inodes(void);
extern void net_format_mac(void);
extern void seldfs_find_file(void);
extern void isr_handler(void);
extern void strcmp(void);
extern void fast_syscall_dispatcher(void);
extern void net_is_online(void);
extern void serial_init(void);
extern void net_tcp_socket_recv(void);
extern void ramdisk_init(void);
extern void elf_validate_header(void);
extern void mouse_handle_byte(void);
extern void boot_anim_draw_logo(void);
extern void pmm_init(void);
extern void sha256_update(void);
extern void rng_get_bytes(void);
extern void serial_puts(void);
extern void vga_print_hex(void);
extern void vga_driver_disable(void);
extern void vga_get_console_rows(void);
extern void vga_enable_fb_console(void);
extern void audio_beep(void);
extern void vga_putchar(void);
extern void pic_mask_irq(void);
extern void pit_sleep_ms(void);
extern void net_download_to_fs(void);
extern void pci_find_class(void);
extern void pit_get_uptime_sec(void);
extern void kmalloc(void);
extern void selftest_pmm(void);
extern void mouse_init(void);
extern void audio_chime_boot(void);
extern void tss_set_rsp0(void);
extern void elf_load_binary(void);
extern void sha256_final(void);
extern void net_http_get(void);
extern void pit_get_uptime_ms(void);
extern void sched_init(void);
extern void vmm_map_page(void);
extern void sha256_hash(void);
extern void strlen(void);
extern void selftest_scheduler(void);
extern void net_send_ip(void);
extern void vga_draw_pixel(void);
extern void net_poll(void);
extern void serial_getchar(void);
extern void seldshell_run(void);
extern void sched_get_current(void);
extern void sched_yield(void);
extern void e1000_get_mac(void);
extern void kernel_panic(void);
extern void pmm_alloc_frames(void);
extern void validate_user_buffer(void);
extern void selftest_kmalloc(void);
extern void irq_register_handler(void);
extern void boot_anim_init(void);
extern void net_send_udp(void);
extern void e1000_init(void);
extern void vga_clear(void);
extern void audio_stop_tone(void);
extern void ram_driver_disable(void);
extern void ram_is_driver_enabled(void);
extern void seldfs_get_file_info(void);

static const struct kernel_symbol g_kernel_symbols[] = {
    { (uint64_t)&seldfs_init, "seldfs_init", "kernel/fs/seldfs.c", 130 },
    { (uint64_t)&vga_get_fb_info, "vga_get_fb_info", "kernel/drivers/vga.c", 383 },
    { (uint64_t)&vmm_get_mapping, "vmm_get_mapping", "kernel/mm/vmm.c", 106 },
    { (uint64_t)&pit_handle_interrupt, "pit_handle_interrupt", "kernel/drivers/pit.c", 44 },
    { (uint64_t)&sched_sleep, "sched_sleep", "kernel/sched/sched.c", 266 },
    { (uint64_t)&seldfs_get_bitmap_stats, "seldfs_get_bitmap_stats", "kernel/fs/seldfs.c", 538 },
    { (uint64_t)&pit_get_ticks, "pit_get_ticks", "kernel/drivers/pit.c", 48 },
    { (uint64_t)&cpu_driver_disable, "cpu_driver_disable", "kernel/sched/sched.c", 300 },
    { (uint64_t)&vga_set_color, "vga_set_color", "kernel/drivers/vga.c", 131 },
    { (uint64_t)&e1000_send_packet, "e1000_send_packet", "kernel/drivers/e1000.c", 319 },
    { (uint64_t)&vga_puts, "vga_puts", "kernel/drivers/vga.c", 353 },
    { (uint64_t)&kbd_init, "kbd_init", "kernel/drivers/kbd.c", 45 },
    { (uint64_t)&sched_tick, "sched_tick", "kernel/sched/sched.c", 136 },
    { (uint64_t)&vmm_destroy_address_space, "vmm_destroy_address_space", "kernel/mm/vmm.c", 141 },
    { (uint64_t)&ata_soft_reset, "ata_soft_reset", "kernel/drivers/ata.c", 180 },
    { (uint64_t)&memmove, "memmove", "kernel/sys/string.c", 12 },
    { (uint64_t)&vga_draw_string_at, "vga_draw_string_at", "kernel/drivers/vga.c", 498 },
    { (uint64_t)&syscall_init_fast, "syscall_init_fast", "kernel/sys/fast_syscall.c", 64 },
    { (uint64_t)&pcnet_send_packet, "pcnet_send_packet", "kernel/drivers/pcnet.c", 258 },
    { (uint64_t)&sched_exit, "sched_exit", "kernel/sched/sched.c", 270 },
    { (uint64_t)&boot_anim_step, "boot_anim_step", "kernel/sys/boot_anim.c", 133 },
    { (uint64_t)&pci_enable_bus_mastering, "pci_enable_bus_mastering", "kernel/drivers/pci.c", 74 },
    { (uint64_t)&rng_init, "rng_init", "kernel/crypto/rand.c", 22 },
    { (uint64_t)&audio_play_tone, "audio_play_tone", "kernel/drivers/audio.c", 91 },
    { (uint64_t)&net_get_arp_table, "net_get_arp_table", "kernel/net/net.c", 229 },
    { (uint64_t)&net_set_config, "net_set_config", "kernel/net/net.c", 1426 },
    { (uint64_t)&seldfs_format, "seldfs_format", "kernel/fs/seldfs.c", 196 },
    { (uint64_t)&jump_to_userspace, "jump_to_userspace", "kernel/sys/fast_syscall.c", 1029 },
    { (uint64_t)&vga_get_humboldt_palette, "vga_get_humboldt_palette", "kernel/drivers/vga.c", 115 },
    { (uint64_t)&sha256_init, "sha256_init", "kernel/crypto/sha256.c", 75 },
    { (uint64_t)&audio_init, "audio_init", "kernel/drivers/audio.c", 252 },
    { (uint64_t)&serial_has_char, "serial_has_char", "kernel/drivers/serial.c", 58 },
    { (uint64_t)&selftest_net, "selftest_net", "kernel/sys/selftest.c", 404 },
    { (uint64_t)&vga_is_gpu_enabled, "vga_is_gpu_enabled", "kernel/drivers/vga.c", 518 },
    { (uint64_t)&e1000_poll_packet, "e1000_poll_packet", "kernel/drivers/e1000.c", 357 },
    { (uint64_t)&net_arp_lookup, "net_arp_lookup", "kernel/net/net.c", 210 },
    { (uint64_t)&vga_set_humboldt_color, "vga_set_humboldt_color", "kernel/drivers/vga.c", 137 },
    { (uint64_t)&vmm_init, "vmm_init", "kernel/mm/vmm.c", 186 },
    { (uint64_t)&pci_write_dword, "pci_write_dword", "kernel/drivers/pci.c", 33 },
    { (uint64_t)&memcpy, "memcpy", "kernel/sys/string.c", 3 },
    { (uint64_t)&pmm_alloc_frame, "pmm_alloc_frame", "kernel/mm/pmm.c", 190 },
    { (uint64_t)&mouse_poll_event, "mouse_poll_event", "kernel/drivers/mouse.c", 174 },
    { (uint64_t)&kernel_panic_in_progress, "kernel_panic_in_progress", "kernel/sys/panic.c", 24 },
    { (uint64_t)&kfree, "kfree", "kernel/mm/kmalloc.c", 154 },
    { (uint64_t)&ata_write_sectors, "ata_write_sectors", "kernel/drivers/ata.c", 397 },
    { (uint64_t)&vga_fill_rect, "vga_fill_rect", "kernel/drivers/vga.c", 467 },
    { (uint64_t)&seldfs_verify_file, "seldfs_verify_file", "kernel/fs/seldfs.c", 439 },
    { (uint64_t)&selftest_user_buffer, "selftest_user_buffer", "kernel/sys/selftest.c", 346 },
    { (uint64_t)&e1000_is_active, "e1000_is_active", "kernel/drivers/e1000.c", 311 },
    { (uint64_t)&kbd_poll_event, "kbd_poll_event", "kernel/drivers/kbd.c", 125 },
    { (uint64_t)&kzalloc, "kzalloc", "kernel/mm/kmalloc.c", 146 },
    { (uint64_t)&seldfs_list_files, "seldfs_list_files", "kernel/fs/seldfs.c", 225 },
    { (uint64_t)&cpu_is_driver_enabled, "cpu_is_driver_enabled", "kernel/sched/sched.c", 308 },
    { (uint64_t)&boot_anim_is_active, "boot_anim_is_active", "kernel/sys/boot_anim.c", 27 },
    { (uint64_t)&net_parse_ip, "net_parse_ip", "kernel/net/net.c", 100 },
    { (uint64_t)&rng_get_u64, "rng_get_u64", "kernel/crypto/rand.c", 52 },
    { (uint64_t)&audio_play_pcm, "audio_play_pcm", "kernel/drivers/audio.c", 276 },
    { (uint64_t)&net_tcp_socket_close, "net_tcp_socket_close", "kernel/net/net.c", 1216 },
    { (uint64_t)&net_tcp_socket_connect, "net_tcp_socket_connect", "kernel/net/net.c", 1100 },
    { (uint64_t)&seldfs_read_file, "seldfs_read_file", "kernel/fs/seldfs.c", 237 },
    { (uint64_t)&gdt_init, "gdt_init", "kernel/sys/gdt.c", 45 },
    { (uint64_t)&pci_read_dword, "pci_read_dword", "kernel/drivers/pci.c", 13 },
    { (uint64_t)&ata_init, "ata_init", "kernel/drivers/ata.c", 229 },
    { (uint64_t)&seldfs_get_inode_by_index, "seldfs_get_inode_by_index", "kernel/fs/seldfs.c", 509 },
    { (uint64_t)&vmm_switch_pml4, "vmm_switch_pml4", "kernel/mm/vmm.c", 31 },
    { (uint64_t)&vga_set_console_rows, "vga_set_console_rows", "kernel/drivers/vga.c", 49 },
    { (uint64_t)&serial_print_dec, "serial_print_dec", "kernel/drivers/serial.c", 42 },
    { (uint64_t)&vga_init, "vga_init", "kernel/drivers/vga.c", 257 },
    { (uint64_t)&selftest_spinlock, "selftest_spinlock", "kernel/sys/selftest.c", 301 },
    { (uint64_t)&selftest_run_all, "selftest_run_all", "kernel/sys/selftest.c", 465 },
    { (uint64_t)&seldfs_write_file, "seldfs_write_file", "kernel/fs/seldfs.c", 326 },
    { (uint64_t)&seldfs_read_file_offset, "seldfs_read_file_offset", "kernel/fs/seldfs.c", 278 },
    { (uint64_t)&pci_read_byte, "pci_read_byte", "kernel/drivers/pci.c", 28 },
    { (uint64_t)&vga_draw_bitmap, "vga_draw_bitmap", "kernel/drivers/vga.c", 480 },
    { (uint64_t)&selftest_sha256, "selftest_sha256", "kernel/sys/selftest.c", 146 },
    { (uint64_t)&sched_create_task, "sched_create_task", "kernel/sched/sched.c", 70 },
    { (uint64_t)&mouse_get_state, "mouse_get_state", "kernel/drivers/mouse.c", 183 },
    { (uint64_t)&net_arp_request, "net_arp_request", "kernel/net/net.c", 240 },
    { (uint64_t)&pci_write_byte, "pci_write_byte", "kernel/drivers/pci.c", 60 },
    { (uint64_t)&pit_init, "pit_init", "kernel/drivers/pit.c", 22 },
    { (uint64_t)&net_checksum, "net_checksum", "kernel/net/net.c", 79 },
    { (uint64_t)&pmm_is_frame_allocated, "pmm_is_frame_allocated", "kernel/mm/pmm.c", 259 },
    { (uint64_t)&idt_init, "idt_init", "kernel/sys/idt.c", 188 },
    { (uint64_t)&kernel_main, "kernel_main", "kernel/kernel.c", 59 },
    { (uint64_t)&rng_has_rdrand, "rng_has_rdrand", "kernel/crypto/rand.c", 41 },
    { (uint64_t)&vga_emergency_text_write, "vga_emergency_text_write", "kernel/drivers/vga.c", 526 },
    { (uint64_t)&kbd_handle_scancode, "kbd_handle_scancode", "kernel/drivers/kbd.c", 54 },
    { (uint64_t)&serial_putchar, "serial_putchar", "kernel/drivers/serial.c", 20 },
    { (uint64_t)&kbd_getchar, "kbd_getchar", "kernel/drivers/kbd.c", 113 },
    { (uint64_t)&vga_init_fb, "vga_init_fb", "kernel/drivers/vga.c", 411 },
    { (uint64_t)&boot_anim_finish, "boot_anim_finish", "kernel/sys/boot_anim.c", 180 },
    { (uint64_t)&pci_read_word, "pci_read_word", "kernel/drivers/pci.c", 23 },
    { (uint64_t)&pci_write_word, "pci_write_word", "kernel/drivers/pci.c", 43 },
    { (uint64_t)&pcnet_init, "pcnet_init", "kernel/drivers/pcnet.c", 87 },
    { (uint64_t)&net_tcp_socket_is_connected, "net_tcp_socket_is_connected", "kernel/net/net.c", 1234 },
    { (uint64_t)&idt_set_gate, "idt_set_gate", "kernel/sys/idt.c", 65 },
    { (uint64_t)&vga_print_dec, "vga_print_dec", "kernel/drivers/vga.c", 367 },
    { (uint64_t)&net_dns_resolve, "net_dns_resolve", "kernel/net/net.c", 950 },
    { (uint64_t)&pcnet_is_active, "pcnet_is_active", "kernel/drivers/pcnet.c", 250 },
    { (uint64_t)&pic_unmask_irq, "pic_unmask_irq", "kernel/sys/idt.c", 110 },
    { (uint64_t)&pmm_free_frames, "pmm_free_frames", "kernel/mm/pmm.c", 247 },
    { (uint64_t)&pci_init, "pci_init", "kernel/drivers/pci.c", 171 },
    { (uint64_t)&ramdisk_get_info, "ramdisk_get_info", "kernel/drivers/ramdisk.c", 82 },
    { (uint64_t)&syscall_dispatch, "syscall_dispatch", "kernel/sys/syscall.c", 28 },
    { (uint64_t)&pcnet_poll_packet, "pcnet_poll_packet", "kernel/drivers/pcnet.c", 288 },
    { (uint64_t)&net_ping, "net_ping", "kernel/net/net.c", 883 },
    { (uint64_t)&vmm_get_kernel_pml4, "vmm_get_kernel_pml4", "kernel/mm/vmm.c", 27 },
    { (uint64_t)&strncmp, "strncmp", "kernel/sys/string.c", 51 },
    { (uint64_t)&net_init, "net_init", "kernel/net/net.c", 1374 },
    { (uint64_t)&seldshell_init, "seldshell_init", "kernel/shell/seldshell.c", 1008 },
    { (uint64_t)&serial_print_hex, "serial_print_hex", "kernel/drivers/serial.c", 34 },
    { (uint64_t)&net_tcp_socket_send, "net_tcp_socket_send", "kernel/net/net.c", 1160 },
    { (uint64_t)&ramdisk_read_sectors, "ramdisk_read_sectors", "kernel/drivers/ramdisk.c", 86 },
    { (uint64_t)&kmalloc_init, "kmalloc_init", "kernel/mm/kmalloc.c", 24 },
    { (uint64_t)&ata_read_sectors, "ata_read_sectors", "kernel/drivers/ata.c", 344 },
    { (uint64_t)&elf_load_and_run, "elf_load_and_run", "kernel/sys/fast_syscall.c", 160 },
    { (uint64_t)&pmm_free_frame, "pmm_free_frame", "kernel/mm/pmm.c", 238 },
    { (uint64_t)&pci_find_device, "pci_find_device", "kernel/drivers/pci.c", 117 },
    { (uint64_t)&seldfs_delete_file, "seldfs_delete_file", "kernel/fs/seldfs.c", 416 },
    { (uint64_t)&memcmp, "memcmp", "kernel/sys/string.c", 63 },
    { (uint64_t)&vga_set_scroll_window, "vga_set_scroll_window", "kernel/drivers/vga.c", 40 },
    { (uint64_t)&mouse_handle_irq, "mouse_handle_irq", "kernel/drivers/mouse.c", 164 },
    { (uint64_t)&pcnet_get_mac, "pcnet_get_mac", "kernel/drivers/pcnet.c", 254 },
    { (uint64_t)&strtoull, "strtoull", "kernel/sys/string.c", 111 },
    { (uint64_t)&sched_get_tasks, "sched_get_tasks", "kernel/sched/sched.c", 286 },
    { (uint64_t)&memset, "memset", "kernel/sys/string.c", 27 },
    { (uint64_t)&seldfs_get_file_count, "seldfs_get_file_count", "kernel/fs/seldfs.c", 529 },
    { (uint64_t)&kernel_panic_exception, "kernel_panic_exception", "kernel/sys/panic.c", 566 },
    { (uint64_t)&ramdisk_write_sectors, "ramdisk_write_sectors", "kernel/drivers/ramdisk.c", 102 },
    { (uint64_t)&kbd_has_char, "kbd_has_char", "kernel/drivers/kbd.c", 108 },
    { (uint64_t)&vmm_unmap_page, "vmm_unmap_page", "kernel/mm/vmm.c", 86 },
    { (uint64_t)&net_format_ip, "net_format_ip", "kernel/net/net.c", 133 },
    { (uint64_t)&seldfs_get_all_inodes, "seldfs_get_all_inodes", "kernel/fs/seldfs.c", 518 },
    { (uint64_t)&net_format_mac, "net_format_mac", "kernel/net/net.c", 163 },
    { (uint64_t)&seldfs_find_file, "seldfs_find_file", "kernel/fs/seldfs.c", 490 },
    { (uint64_t)&isr_handler, "isr_handler", "kernel/sys/idt.c", 140 },
    { (uint64_t)&strcmp, "strcmp", "kernel/sys/string.c", 43 },
    { (uint64_t)&fast_syscall_dispatcher, "fast_syscall_dispatcher", "kernel/sys/fast_syscall.c", 276 },
    { (uint64_t)&net_is_online, "net_is_online", "kernel/net/net.c", 1418 },
    { (uint64_t)&serial_init, "serial_init", "kernel/drivers/serial.c", 6 },
    { (uint64_t)&net_tcp_socket_recv, "net_tcp_socket_recv", "kernel/net/net.c", 1184 },
    { (uint64_t)&ramdisk_init, "ramdisk_init", "kernel/drivers/ramdisk.c", 21 },
    { (uint64_t)&elf_validate_header, "elf_validate_header", "kernel/sys/elf.c", 17 },
    { (uint64_t)&mouse_handle_byte, "mouse_handle_byte", "kernel/drivers/mouse.c", 109 },
    { (uint64_t)&boot_anim_draw_logo, "boot_anim_draw_logo", "kernel/sys/boot_anim.c", 47 },
    { (uint64_t)&pmm_init, "pmm_init", "kernel/mm/pmm.c", 83 },
    { (uint64_t)&sha256_update, "sha256_update", "kernel/crypto/sha256.c", 87 },
    { (uint64_t)&rng_get_bytes, "rng_get_bytes", "kernel/crypto/rand.c", 73 },
    { (uint64_t)&serial_puts, "serial_puts", "kernel/drivers/serial.c", 25 },
    { (uint64_t)&vga_print_hex, "vga_print_hex", "kernel/drivers/vga.c", 359 },
    { (uint64_t)&vga_driver_disable, "vga_driver_disable", "kernel/drivers/vga.c", 506 },
    { (uint64_t)&vga_get_console_rows, "vga_get_console_rows", "kernel/drivers/vga.c", 59 },
    { (uint64_t)&vga_enable_fb_console, "vga_enable_fb_console", "kernel/drivers/vga.c", 221 },
    { (uint64_t)&audio_beep, "audio_beep", "kernel/drivers/audio.c", 118 },
    { (uint64_t)&vga_putchar, "vga_putchar", "kernel/drivers/vga.c", 274 },
    { (uint64_t)&pic_mask_irq, "pic_mask_irq", "kernel/sys/idt.c", 124 },
    { (uint64_t)&pit_sleep_ms, "pit_sleep_ms", "kernel/drivers/pit.c", 60 },
    { (uint64_t)&net_download_to_fs, "net_download_to_fs", "kernel/net/net.c", 1319 },
    { (uint64_t)&pci_find_class, "pci_find_class", "kernel/drivers/pci.c", 143 },
    { (uint64_t)&pit_get_uptime_sec, "pit_get_uptime_sec", "kernel/drivers/pit.c", 56 },
    { (uint64_t)&kmalloc, "kmalloc", "kernel/mm/kmalloc.c", 87 },
    { (uint64_t)&selftest_pmm, "selftest_pmm", "kernel/sys/selftest.c", 179 },
    { (uint64_t)&mouse_init, "mouse_init", "kernel/drivers/mouse.c", 55 },
    { (uint64_t)&audio_chime_boot, "audio_chime_boot", "kernel/drivers/audio.c", 125 },
    { (uint64_t)&tss_set_rsp0, "tss_set_rsp0", "kernel/sys/gdt.c", 41 },
    { (uint64_t)&elf_load_binary, "elf_load_binary", "kernel/sys/elf.c", 62 },
    { (uint64_t)&sha256_final, "sha256_final", "kernel/crypto/sha256.c", 115 },
    { (uint64_t)&net_http_get, "net_http_get", "kernel/net/net.c", 1239 },
    { (uint64_t)&pit_get_uptime_ms, "pit_get_uptime_ms", "kernel/drivers/pit.c", 52 },
    { (uint64_t)&sched_init, "sched_init", "kernel/sched/sched.c", 46 },
    { (uint64_t)&vmm_map_page, "vmm_map_page", "kernel/mm/vmm.c", 35 },
    { (uint64_t)&sha256_hash, "sha256_hash", "kernel/crypto/sha256.c", 140 },
    { (uint64_t)&strlen, "strlen", "kernel/sys/string.c", 35 },
    { (uint64_t)&selftest_scheduler, "selftest_scheduler", "kernel/sys/selftest.c", 272 },
    { (uint64_t)&net_send_ip, "net_send_ip", "kernel/net/net.c", 807 },
    { (uint64_t)&vga_draw_pixel, "vga_draw_pixel", "kernel/drivers/vga.c", 459 },
    { (uint64_t)&net_poll, "net_poll", "kernel/net/net.c", 776 },
    { (uint64_t)&serial_getchar, "serial_getchar", "kernel/drivers/serial.c", 62 },
    { (uint64_t)&seldshell_run, "seldshell_run", "kernel/shell/seldshell.c", 1014 },
    { (uint64_t)&sched_get_current, "sched_get_current", "kernel/sched/sched.c", 282 },
    { (uint64_t)&sched_yield, "sched_yield", "kernel/sched/sched.c", 195 },
    { (uint64_t)&e1000_get_mac, "e1000_get_mac", "kernel/drivers/e1000.c", 315 },
    { (uint64_t)&kernel_panic, "kernel_panic", "kernel/sys/panic.c", 562 },
    { (uint64_t)&pmm_alloc_frames, "pmm_alloc_frames", "kernel/mm/pmm.c", 207 },
    { (uint64_t)&validate_user_buffer, "validate_user_buffer", "kernel/sys/syscall.c", 6 },
    { (uint64_t)&selftest_kmalloc, "selftest_kmalloc", "kernel/sys/selftest.c", 45 },
    { (uint64_t)&irq_register_handler, "irq_register_handler", "kernel/sys/idt.c", 104 },
    { (uint64_t)&boot_anim_init, "boot_anim_init", "kernel/sys/boot_anim.c", 97 },
    { (uint64_t)&net_send_udp, "net_send_udp", "kernel/net/net.c", 930 },
    { (uint64_t)&e1000_init, "e1000_init", "kernel/drivers/e1000.c", 111 },
    { (uint64_t)&vga_clear, "vga_clear", "kernel/drivers/vga.c", 207 },
    { (uint64_t)&audio_stop_tone, "audio_stop_tone", "kernel/drivers/audio.c", 113 },
    { (uint64_t)&ram_driver_disable, "ram_driver_disable", "kernel/mm/pmm.c", 278 },
    { (uint64_t)&ram_is_driver_enabled, "ram_is_driver_enabled", "kernel/mm/pmm.c", 285 },
    { (uint64_t)&seldfs_get_file_info, "seldfs_get_file_info", "kernel/fs/seldfs.c", 479 },
};

#define G_KERNEL_SYMBOLS_COUNT (sizeof(g_kernel_symbols) / sizeof(g_kernel_symbols[0]))

const struct kernel_symbol* ksym_lookup(uint64_t rip, uint64_t* out_offset) {
    const struct kernel_symbol* best = NULL;
    uint64_t best_diff = (uint64_t)-1;

    for (size_t i = 0; i < G_KERNEL_SYMBOLS_COUNT; i++) {
        uint64_t sym_addr = g_kernel_symbols[i].addr;
        if (rip >= sym_addr) {
            uint64_t diff = rip - sym_addr;
            if (diff < best_diff && diff < 0x8000) {
                best_diff = diff;
                best = &g_kernel_symbols[i];
            }
        }
    }
    if (out_offset) {
        *out_offset = (best != NULL) ? best_diff : 0;
    }
    return best;
}
