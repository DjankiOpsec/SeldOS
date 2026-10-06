/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * POSIX Standard Symbolic Constants & Process Primitives
 * GPLv3 Licensed.
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <stddef.h>
#include <stdint.h>
#include "seld.h"

/* Standard POSIX File Descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* File Access & Creation Flags */
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

/* Lseek whence values */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef long ssize_t;
typedef long off_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;

/* Process & Execution */
int      getpid(void);
uint64_t uptime(void);
void     yield(void);
int      spawn(const char* path);
int      spawnv(const char* path, char* const argv[]);
unsigned int sleep(unsigned int seconds);
int      usleep(uint64_t usec);
int      gettasks(struct snl_task_info* tasks, size_t max_tasks);
int      meminfo(struct snl_meminfo* info);
int      clear(void);

/* Memory segment management */
int      brk(void* addr);
void*    sbrk(intptr_t increment);

/* File I/O and Filesystem */
ssize_t  read(int fd, void* buf, size_t count);
ssize_t  write(int fd, const void* buf, size_t count);
int      open(const char* pathname, int flags, ...);
int      close(int fd);
int      stat(const char* pathname, struct seld_stat* statbuf);
int      listdir(const char* path, struct seld_dirent* dirents, size_t max_entries);
int      unlink(const char* pathname);
int      readfile(const char* path, void* buf, size_t max_len);
int      writefile(const char* path, const void* buf, size_t len);
off_t    lseek(int fd, off_t offset, int whence);
void     reboot(void);
void     poweroff(void);

#endif /* _UNISTD_H_ */
