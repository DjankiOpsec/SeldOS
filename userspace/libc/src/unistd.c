/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * POSIX System Interface Implementation
 * GPLv3 Licensed.
 */

#include "unistd.h"
#include "seld.h"
#include <stdarg.h>

int getpid(void) {
    return seld_getpid();
}

uint64_t uptime(void) {
    return seld_uptime();
}

void yield(void) {
    seld_yield();
}

ssize_t read(int fd, void* buf, size_t count) {
    return (ssize_t)seld_read(fd, buf, count);
}

ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)seld_write(fd, buf, count);
}

int open(const char* pathname, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return seld_open(pathname, flags, mode);
}

int close(int fd) {
    return seld_close(fd);
}

int spawn(const char* path) {
    return seld_spawn(path);
}

int spawnv(const char* path, char* const argv[]) {
    return seld_spawnv(path, argv);
}

int listdir(const char* path, struct seld_dirent* dirents, size_t max_entries) {
    return seld_listdir(path, dirents, max_entries);
}

int stat(const char* pathname, struct seld_stat* statbuf) {
    return seld_stat(pathname, statbuf);
}

int unlink(const char* pathname) {
    return seld_unlink(pathname);
}

int readfile(const char* path, void* buf, size_t max_len) {
    return seld_readfile(path, buf, max_len);
}

int writefile(const char* path, const void* buf, size_t len) {
    return seld_writefile(path, buf, len);
}

int gettasks(struct snl_task_info* tasks, size_t max_tasks) {
    return seld_gettasks(tasks, max_tasks);
}

int meminfo(struct snl_meminfo* info) {
    return seld_meminfo(info);
}

int clear(void) {
    return seld_clear();
}

unsigned int sleep(unsigned int seconds) {
    seld_sleep((uint64_t)seconds * 1000);
    return 0;
}

int usleep(uint64_t usec) {
    seld_sleep((usec + 999) / 1000);
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)seld_lseek(fd, (long)offset, whence);
}
