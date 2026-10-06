/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Native SNL Syscall Interface & Structures
 * GPLv3 Licensed.
 */

#ifndef _SELD_H_
#define _SELD_H_

#include <stdint.h>
#include <stddef.h>

/* Fast System Call Vector Numbers */
#define SYS_EXIT        1
#define SYS_WRITE       2
#define SYS_READ        3
#define SYS_YIELD       4
#define SYS_UPTIME      5
#define SYS_GETPID      6
#define SYS_SELD_VERIFY 7
#define SYS_OPEN        10
#define SYS_CLOSE       11
#define SYS_STAT        12
#define SYS_LISTDIR     13
#define SYS_BRK         14
#define SYS_SPAWN       15
#define SYS_EXEC        16
#define SYS_SLEEP       17
#define SYS_UNLINK      18
#define SYS_GETTASKS    19
#define SYS_MEMINFO     20
#define SYS_CLEAR       21
#define SYS_READFILE    22
#define SYS_WRITEFILE   23
#define SYS_LSEEK       24
#define SYS_FRAMEBUFFER 25
#define SYS_POLLKEY     26
#define SYS_POLLMOUSE   27
#define SYS_BEEP        28
#define SYS_AUDIO_PLAY  29
#define SYS_SET_CONSOLE_ROWS 30
#define SYS_NET_INFO    31
#define SYS_NET_PING    32
#define SYS_NET_ARP     33
#define SYS_NET_DOWNLOAD    34
#define SYS_NET_TCP_CONNECT 35
#define SYS_NET_TCP_SEND    36
#define SYS_NET_TCP_RECV    37
#define SYS_NET_TCP_CLOSE   38
#define SYS_NET_DNS_RESOLVE 39
#define SYS_DRV_OFF         40
#define SYS_SELD            42
#define SYS_REBOOT          43
#define SYS_POWEROFF        44
#define SYS_IMMUNE_PURGE    45
#define SYS_NET_SET_LOCK    46
#define SYS_NET_GET_LOCK    47
#define SYS_PLEDGE          48
#define SYS_NET_SET_DESYNC  49
#define SYS_NET_GET_DESYNC  50
#define SYS_UNVEIL          51
#define SYS_OPSEC_SET_JITTER 52
#define SYS_OPSEC_GET_JITTER 53

/* Seld-Unveil Permission Flags (OpenBSD-style Ring 3 Filesystem Sandboxing) */
#define UNVEIL_READ         0x01  /* Read file: open(O_RDONLY), readfile, stat */
#define UNVEIL_WRITE        0x02  /* Write file: open(O_WRONLY/O_RDWR), writefile */
#define UNVEIL_EXEC         0x04  /* Execute binary: spawn, exec */
#define UNVEIL_CREATE       0x08  /* Create/delete file: open(O_CREAT), unlink */

/* Seld-Pledge Capability Flags (OpenBSD-style Ring 3 Syscall Sandboxing) */
#define PLEDGE_STDIO        (1 << 0)  /* Basic stdio, heap, exit, yield, uptime, screen, input */
#define PLEDGE_RPATH        (1 << 1)  /* Filesystem read: open, read, stat, listdir, readfile */
#define PLEDGE_WPATH        (1 << 2)  /* Filesystem write/delete: writefile, unlink */
#define PLEDGE_EXEC         (1 << 3)  /* Process execution: spawn, exec */
#define PLEDGE_NET          (1 << 4)  /* Networking: tcp_connect, tcp_send, tcp_recv, tcp_close, ping, arp */
#define PLEDGE_DNS          (1 << 5)  /* Domain name resolution: dns_resolve */
#define PLEDGE_AUDIO        (1 << 6)  /* Audio hardware: beep, audio_play */
#define PLEDGE_PURGE        (1 << 7)  /* Immune system purge and air-gap shield controls */
#define PLEDGE_REBOOT       (1 << 8)  /* Hardware reboot and poweroff */

#define NET_AIRGAP_UNLOCKED 0
#define NET_AIRGAP_LOCKED   1
#define NET_AIRGAP_ONDEMAND 2
#define NET_LEASE_ACQUIRE   3
#define NET_LEASE_RELEASE   4

/* File types */
#define SELD_FILE_REGULAR 1
#define SELD_FILE_DIR     2

/* SeldOS Stat Structure */
struct seld_stat {
    char     name[32];
    uint32_t size;
    uint32_t block_count;
    uint32_t flags;
    uint8_t  sha256[32];
};

/* SeldOS Directory Entry Structure */
struct seld_dirent {
    char     name[32];
    uint32_t size;
    uint32_t block_count;
    uint8_t  sha256[32];
};

/* SeldOS Task Info Structure */
struct snl_task_info {
    uint32_t id;
    char     name[24];
    uint32_t state;
    uint64_t rsp;
    void*    stack_base;
    uint64_t sleep_until_ticks;
    uint64_t runtime_ticks;
};

/* SeldOS Memory Info Structure */
struct snl_meminfo {
    uint64_t total_ram;
    uint64_t used_ram;
    uint64_t free_ram;
    uint64_t heap_size;
    uint64_t heap_used;
    uint64_t heap_free;
};

#define HUMBOLDT_PALETTE_SIZE 39
#define HUMBOLDT_FB_WIDTH     680
#define HUMBOLDT_FB_HEIGHT    334

/* Framebuffer Info Structure */
struct seld_fb_info {
    void*    framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
};

/* Keyboard Event Structure */
struct seld_kbd_event {
    uint8_t scancode;
    uint8_t pressed;
};

/* Mouse Event Structure */
struct seld_mouse_event {
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
};

/* Network Structures */
struct seld_net_info {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t  mac[6];
    uint8_t  link_up;
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_dropped;
    uint64_t rx_checksum_errors;
};

struct seld_arp_entry {
    uint32_t ip;
    uint8_t  mac[6];
    uint8_t  valid;
    uint64_t timestamp_ms;
};

/* Fast syscall assembly invocation wrappers */
long seld_syscall(long num, long arg1, long arg2, long arg3);
long seld_syscall4(long num, long arg1, long arg2, long arg3, long arg4);

/* Native SNL syscall wrappers */
void     seld_exit(int status) __attribute__((noreturn));
long     seld_write(int fd, const void* buf, size_t count);
long     seld_read(int fd, void* buf, size_t count);
void     seld_yield(void);
uint64_t seld_uptime(void);
int      seld_getpid(void);
long     seld_verify(long code, long arg);
int      seld_open(const char* path, int flags, int mode);
int      seld_close(int fd);
int      seld_stat(const char* path, struct seld_stat* st);
int      seld_listdir(const char* path, struct seld_dirent* dirents, size_t max_entries);
void*    seld_brk(void* addr);
int      seld_spawn(const char* path);
int      seld_spawnv(const char* path, char* const argv[]);
void     seld_sleep(uint64_t ms);
int      seld_unlink(const char* path);
int      seld_gettasks(struct snl_task_info* tasks, size_t max_tasks);
int      seld_meminfo(struct snl_meminfo* info);
int      seld_clear(void);
int      seld_set_console_rows(int rows);
int      seld_readfile(const char* path, void* buf, size_t max_len);
int      seld_writefile(const char* path, const void* buf, size_t len);
long     seld_lseek(int fd, long offset, int whence);
int      seld_get_framebuffer(struct seld_fb_info* fb);
int      seld_poll_key(struct seld_kbd_event* ev);
int      seld_poll_mouse(struct seld_mouse_event* ev);
int      seld_beep(uint32_t freq_hz, uint32_t duration_ms);
int      seld_snd(int8_t ona);
uint32_t seld_ona2freq(int8_t ona);
int8_t   seld_freq2ona(uint32_t freq_hz);
int      seld_audio_play(const void* samples, size_t len, uint32_t sample_rate);
long     seld_ping(void);
int      seld_net_info(struct seld_net_info* info);
int      seld_net_ping(uint32_t ip, uint16_t seq, uint32_t* rtt_ms);
int      seld_net_arp(struct seld_arp_entry* entries, size_t max_entries);
int      seld_net_download(uint32_t ip, uint16_t port, const char* url_path, const char* local_path);
int      seld_download_url(const char* url, const char* local_path);
int      seld_https_download(const char* host, const char* path, const char* local_path);
int      seld_tcp_connect(uint32_t ip, uint16_t port);
int      seld_tcp_send(int sock, const void* data, size_t len);
int      seld_tcp_recv(int sock, void* buf, size_t max_len, uint32_t timeout_ms);
int      seld_tcp_close(int sock);
/* Control D Sovereign OpSec DNS Profile (Ads & Trackers - p2) */
#define SELD_MAKE_IP(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define SELD_CONTROLD_DNS_P2_PRIMARY   SELD_MAKE_IP(76, 76, 2, 2)
#define SELD_CONTROLD_DNS_P2_SECONDARY SELD_MAKE_IP(76, 76, 10, 2)
#define SELD_CONTROLD_DOT_IP           SELD_MAKE_IP(76, 76, 2, 11)
#define SELD_CONTROLD_DOT_HOST         "p2.freedns.controld.com"
#define SELD_CONTROLD_DOH_URL          "https://freedns.controld.com/p2"

#define SELD_AIRGAP_UNLOCKED 0
#define SELD_AIRGAP_LOCKED   1
#define SELD_AIRGAP_ONDEMAND 2
#define SELD_AIRGAP_STEALTH  3

#define SELD_DESYNC_NONE     0
#define SELD_DESYNC_SPLIT    1
#define SELD_DESYNC_FAKE     2

int      seld_dns_resolve(const char* hostname, uint32_t* ip_out);
int      seld_dns_resolve_dot(const char* hostname, uint32_t* ip_out);
int      seld_drv_off(const char* driver_name);
int      seld_reboot(void);
int      seld_poweroff(void);
int      seld_immune_purge(void);
int      seld_net_lock(int mode);
int      seld_net_get_lock(void);
int      seld_net_lease_acquire(void);
int      seld_net_lease_release(void);
int      seld_net_set_desync(int mode);
int      seld_net_get_desync(void);
int      seld_pledge(uint32_t flags);
int      seld_unveil(const char* path, const char* permissions);
int      seld_set_jitter(int enable);
int      seld_get_jitter(void);

#endif /* _SELD_H_ */
