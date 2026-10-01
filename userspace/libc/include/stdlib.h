/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * General Utilities & Dynamic Memory Management
 * GPLv3 Licensed.
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>
#include <stdint.h>

/* Dynamic Memory Allocation */
void* malloc(size_t size);
void  free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t nmemb, size_t size);

/* Process Termination */
void  exit(int status) __attribute__((noreturn));

/* Conversions and Arithmetic */
int   atoi(const char* nptr);
double atof(const char* nptr);
char* itoa(int value, char* str, int base);
int   abs(int j);
long  labs(long j);
int   system(const char* command);

/* Environment */
char* getenv(const char* name);

#endif /* _STDLIB_H_ */
