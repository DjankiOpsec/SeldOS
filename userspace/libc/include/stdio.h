/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Standard Input / Output Definitions & Buffered Streams
 * GPLv3 Licensed.
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "seld.h"
#include "unistd.h"
#include "stdlib.h"
#include "string.h"

#define EOF (-1)
#define BUFSIZ 512

/* Stream buffering types */
#define _IONBF 0  /* Unbuffered */
#define _IOLBF 1  /* Line buffered */
#define _IOFBF 2  /* Fully buffered */

/* Stream flags */
#define _FILE_READ  0x01
#define _FILE_WRITE 0x02
#define _FILE_RDWR  0x04

typedef struct FILE {
    int  fd;
    int  flags;          /* Read/write permissions */
    int  mode;           /* Buffering mode */
    char buf[BUFSIZ];    /* Internal stream buffer */
    int  buf_pos;        /* Current offset inside buffer */
    int  buf_size;       /* Available valid bytes in buffer */
    int  eof;            /* EOF indicator */
    int  error;          /* Error indicator */
    int  is_static;      /* 1 for stdin/stdout/stderr, 0 for fopen */
} FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

/* Formatted Output */
int printf(const char* format, ...) __attribute__((format(printf, 1, 2)));
int sprintf(char* str, const char* format, ...) __attribute__((format(printf, 2, 3)));
int snprintf(char* str, size_t size, const char* format, ...) __attribute__((format(printf, 3, 4)));
int vprintf(const char* format, va_list ap);
int vsprintf(char* str, const char* format, va_list ap);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int fprintf(FILE* stream, const char* format, ...) __attribute__((format(printf, 2, 3)));
int vfprintf(FILE* stream, const char* format, va_list ap);

/* Character and String I/O */
int   puts(const char* s);
int   putchar(int c);
int   getchar(void);
int   fputs(const char* s, FILE* stream);
char* fgets(char* s, int size, FILE* stream);
int   fputc(int c, FILE* stream);
int   fgetc(FILE* stream);

/* Stream File Control */
FILE*  fopen(const char* filename, const char* mode);
int    fclose(FILE* stream);
int    remove(const char* pathname);
int    rename(const char* oldpath, const char* newpath);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int    fseek(FILE* stream, long offset, int whence);
long   ftell(FILE* stream);
void   rewind(FILE* stream);
int    fflush(FILE* stream);
int    feof(FILE* stream);
int    ferror(FILE* stream);
int    sscanf(const char* str, const char* format, ...);

/* Compatibility functions for Humboldt kernel/shell tests */
void print_hex(uint64_t val);
void print_dec(uint64_t val);

#endif /* _STDIO_H_ */
