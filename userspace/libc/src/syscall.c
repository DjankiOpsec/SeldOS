/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Fast SYSCALL / SYSRET Assembly Wrappers
 * GPLv3 Licensed.
 */

#include "seld.h"
#include <string.h>

long seld_syscall(long num, long arg1, long arg2, long arg3) {
    long ret;
    register long r10 __asm__("r10") = 0;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long seld_syscall4(long num, long arg1, long arg2, long arg3, long arg4) {
    long ret;
    register long r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void seld_exit(int status) {
    seld_syscall(SYS_EXIT, (long)status, 0, 0);
    while (1) {
        __asm__ volatile ("pause");
    }
}

long seld_write(int fd, const void* buf, size_t count) {
    return seld_syscall(SYS_WRITE, (long)fd, (long)buf, (long)count);
}

long seld_read(int fd, void* buf, size_t count) {
    return seld_syscall(SYS_READ, (long)fd, (long)buf, (long)count);
}

void seld_yield(void) {
    seld_syscall(SYS_YIELD, 0, 0, 0);
}

uint64_t seld_uptime(void) {
    return (uint64_t)seld_syscall(SYS_UPTIME, 0, 0, 0);
}

int seld_getpid(void) {
    return (int)seld_syscall(SYS_GETPID, 0, 0, 0);
}

long seld_verify(long code, long arg) {
    return seld_syscall(SYS_SELD_VERIFY, code, arg, 0);
}

int seld_open(const char* path, int flags, int mode) {
    return (int)seld_syscall(SYS_OPEN, (long)path, (long)flags, (long)mode);
}

int seld_close(int fd) {
    return (int)seld_syscall(SYS_CLOSE, (long)fd, 0, 0);
}

int seld_stat(const char* path, struct seld_stat* st) {
    return (int)seld_syscall(SYS_STAT, (long)path, (long)st, 0);
}

int seld_listdir(const char* path, struct seld_dirent* dirents, size_t max_entries) {
    return (int)seld_syscall(SYS_LISTDIR, (long)path, (long)dirents, (long)max_entries);
}

void* seld_brk(void* addr) {
    return (void*)seld_syscall(SYS_BRK, (long)addr, 0, 0);
}

int seld_spawn(const char* path) {
    char* argv[2] = {(char*)path, NULL};
    return seld_spawnv(path, argv);
}

int seld_spawnv(const char* path, char* const argv[]) {
    return (int)seld_syscall(SYS_SPAWN, (long)path, (long)argv, 0);
}

void seld_sleep(uint64_t ms) {
    seld_syscall(SYS_SLEEP, (long)ms, 0, 0);
}

int seld_unlink(const char* path) {
    return (int)seld_syscall(SYS_UNLINK, (long)path, 0, 0);
}

int seld_gettasks(struct snl_task_info* tasks, size_t max_tasks) {
    return (int)seld_syscall(SYS_GETTASKS, (long)tasks, (long)max_tasks, 0);
}

int seld_meminfo(struct snl_meminfo* info) {
    return (int)seld_syscall(SYS_MEMINFO, (long)info, 0, 0);
}

int seld_clear(void) {
    return (int)seld_syscall(SYS_CLEAR, 0, 0, 0);
}

int seld_set_console_rows(int rows) {
    return (int)seld_syscall(SYS_SET_CONSOLE_ROWS, (long)rows, 0, 0);
}

int seld_readfile(const char* path, void* buf, size_t max_len) {
    return (int)seld_syscall(SYS_READFILE, (long)path, (long)buf, (long)max_len);
}

int seld_writefile(const char* path, const void* buf, size_t len) {
    return (int)seld_syscall(SYS_WRITEFILE, (long)path, (long)buf, (long)len);
}

long seld_lseek(int fd, long offset, int whence) {
    return seld_syscall(SYS_LSEEK, (long)fd, offset, (long)whence);
}

int seld_get_framebuffer(struct seld_fb_info* fb) {
    return (int)seld_syscall(SYS_FRAMEBUFFER, (long)fb, 0, 0);
}

int seld_poll_key(struct seld_kbd_event* ev) {
    return (int)seld_syscall(SYS_POLLKEY, (long)ev, 0, 0);
}

int seld_poll_mouse(struct seld_mouse_event* ev) {
    return (int)seld_syscall(SYS_POLLMOUSE, (long)ev, 0, 0);
}

int seld_beep(uint32_t freq_hz, uint32_t duration_ms) {
    return (int)seld_syscall(SYS_BEEP, (long)freq_hz, (long)duration_ms, 0);
}

int seld_audio_play(const void* samples, size_t len, uint32_t sample_rate) {
    return (int)seld_syscall(SYS_AUDIO_PLAY, (long)samples, (long)len, (long)sample_rate);
}

long seld_ping(void) {
    return seld_syscall(SYS_SELD, 0, 0, 0);
}

int seld_net_info(struct seld_net_info* info) {
    return (int)seld_syscall(SYS_NET_INFO, (long)info, 0, 0);
}

int seld_net_ping(uint32_t ip, uint16_t seq, uint32_t* rtt_ms) {
    return (int)seld_syscall(SYS_NET_PING, (long)ip, (long)seq, (long)rtt_ms);
}

int seld_net_arp(struct seld_arp_entry* entries, size_t max_entries) {
    return (int)seld_syscall(SYS_NET_ARP, (long)entries, (long)max_entries, 0);
}

int seld_net_download(uint32_t ip, uint16_t port, const char* url_path, const char* local_path) {
    return (int)seld_syscall4(SYS_NET_DOWNLOAD, (long)ip, (long)port, (long)url_path, (long)local_path);
}

int seld_download_url(const char* url, const char* local_path) {
    if (!url || !local_path) return -1;

    if (strcmp(url, "tor") == 0 || strcmp(url, "torbrowser") == 0) {
        return seld_net_download(0, 8080, "/tor", local_path);
    }

    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    char host_str[64];
    size_t hidx = 0;
    while (*p && *p != ':' && *p != '/' && hidx < sizeof(host_str) - 1) {
        host_str[hidx++] = *p++;
    }
    host_str[hidx] = '\0';

    uint16_t port = 80;
    if (*p == ':') {
        p++;
        port = 0;
        while (*p >= '0' && *p <= '9') {
            port = (uint16_t)(port * 10 + (*p++ - '0'));
        }
    }

    const char* path = (*p == '/') ? p : "/";

    uint32_t ip = 0;
    if (strcmp(host_str, "localhost") == 0 || strcmp(host_str, "127.0.0.1") == 0 || strcmp(host_str, "gateway") == 0) {
        ip = 0;
    } else {
        unsigned int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
        const char* hp = host_str;
        while (*hp >= '0' && *hp <= '9') o1 = o1 * 10 + (*hp++ - '0');
        if (*hp == '.') hp++;
        while (*hp >= '0' && *hp <= '9') o2 = o2 * 10 + (*hp++ - '0');
        if (*hp == '.') hp++;
        while (*hp >= '0' && *hp <= '9') o3 = o3 * 10 + (*hp++ - '0');
        if (*hp == '.') hp++;
        while (*hp >= '0' && *hp <= '9') o4 = o4 * 10 + (*hp++ - '0');

        if (o1 <= 255 && o2 <= 255 && o3 <= 255 && o4 <= 255 && hp > host_str) {
            ip = (uint32_t)(o1 | (o2 << 8) | (o3 << 16) | (o4 << 24));
        } else {
            seld_dns_resolve(host_str, &ip);
        }
    }

    return seld_net_download(ip, port, path, local_path);
}

int seld_tcp_connect(uint32_t ip, uint16_t port) {
    return (int)seld_syscall(SYS_NET_TCP_CONNECT, (long)ip, (long)port, 0);
}

int seld_tcp_send(int sock, const void* data, size_t len) {
    return (int)seld_syscall(SYS_NET_TCP_SEND, (long)sock, (long)data, (long)len);
}

int seld_tcp_recv(int sock, void* buf, size_t max_len, uint32_t timeout_ms) {
    return (int)seld_syscall4(SYS_NET_TCP_RECV, (long)sock, (long)buf, (long)max_len, (long)timeout_ms);
}

int seld_tcp_close(int sock) {
    return (int)seld_syscall(SYS_NET_TCP_CLOSE, (long)sock, 0, 0);
}

int seld_dns_resolve(const char* hostname, uint32_t* ip_out) {
    if (!hostname || !ip_out) return -1;
    return (int)seld_syscall(SYS_NET_DNS_RESOLVE, (long)hostname, (long)ip_out, 0);
}
