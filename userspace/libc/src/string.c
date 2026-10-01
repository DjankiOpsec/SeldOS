/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Standard String and Memory Operations
 * GPLv3 Licensed.
 */

#include "string.h"
#include "stdlib.h"

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0') {}
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d != '\0') {
        d++;
    }
    while ((*d++ = *src++) != '\0') {}
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d != '\0') {
        d++;
    }
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        *d++ = src[i++];
    }
    *d = '\0';
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 != '\0' && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (int)((unsigned char)s1[i] - (unsigned char)s2[i]);
        }
    }
    return 0;
}

static inline char to_lower_char(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 != '\0' && (to_lower_char(*s1) == to_lower_char(*s2))) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)to_lower_char(*s1) - (unsigned char)to_lower_char(*s2));
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = to_lower_char(s1[i]);
        char c2 = to_lower_char(s2[i]);
        if (c1 != c2 || s1[i] == '\0') {
            return (int)((unsigned char)c1 - (unsigned char)c2);
        }
    }
    return 0;
}

char* strchr(const char* s, int c) {
    char ch = (char)c;
    while (*s != '\0') {
        if (*s == ch) {
            return (char*)s;
        }
        s++;
    }
    return (ch == '\0') ? (char*)s : NULL;
}

char* strrchr(const char* s, int c) {
    char ch = (char)c;
    const char* last = NULL;
    while (*s != '\0') {
        if (*s == ch) {
            last = s;
        }
        s++;
    }
    if (ch == '\0') {
        return (char*)s;
    }
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack != '\0'; haystack++) {
        if (*haystack == *needle) {
            const char* h = haystack;
            const char* n = needle;
            while (*h != '\0' && *n != '\0' && (*h == *n)) {
                h++;
                n++;
            }
            if (*n == '\0') {
                return (char*)haystack;
            }
        }
    }
    return NULL;
}

char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* dup = (char*)malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len + 1);
    return dup;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    unsigned char uc = (unsigned char)c;
    for (size_t i = 0; i < n; i++) {
        p[i] = uc;
    }
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)(p1[i] - p2[i]);
        }
    }
    return 0;
}
