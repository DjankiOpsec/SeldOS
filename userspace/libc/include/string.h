/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * String and Memory Handling Definitions
 * GPLv3 Licensed.
 */

#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>
#include <stdint.h>

/* String Inspection and Manipulation */
size_t strlen(const char* s);
char*  strcpy(char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t n);
char*  strcat(char* dest, const char* src);
char*  strncat(char* dest, const char* src, size_t n);
int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
int    strcasecmp(const char* s1, const char* s2);
int    strncasecmp(const char* s1, const char* s2, size_t n);
char*  strchr(const char* s, int c);
char*  strrchr(const char* s, int c);
char*  strstr(const char* haystack, const char* needle);
char*  strdup(const char* s);

/* Memory Buffers */
void*  memset(void* s, int c, size_t n);
void*  memcpy(void* dest, const void* src, size_t n);
void*  memmove(void* dest, const void* src, size_t n);
int    memcmp(const void* s1, const void* s2, size_t n);

#endif /* _STRING_H_ */
