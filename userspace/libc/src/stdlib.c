/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Standard Utilities (exit, atoi, itoa, abs, getenv)
 * GPLv3 Licensed.
 */

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "seld.h"

void exit(int status) {
    fflush(stdout);
    fflush(stderr);
    seld_exit(status);
}

int atoi(const char* nptr) {
    if (!nptr) return 0;

    /* Skip leading whitespace */
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' ||
           *nptr == '\r' || *nptr == '\f' || *nptr == '\v') {
        nptr++;
    }

    int sign = 1;
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    uint32_t result = 0;
    while (*nptr >= '0' && *nptr <= '9') {
        uint32_t digit = (uint32_t)(*nptr - '0');
        // Prevent signed integer multiplication/addition overflow
        if (result > (2147483647U - digit) / 10U) {
            return (sign == 1) ? 2147483647 : (-2147483647 - 1);
        }
        result = result * 10U + digit;
        nptr++;
    }

    return (int)result * sign;
}

int parse_uint32_safe(const char* str, uint32_t* out_val) {
    if (!str || !out_val) return -1;

    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }

    // Explicitly reject negative numbers to prevent signed-to-unsigned wrap-around
    if (*str == '-') {
        return -2;
    }
    if (*str == '+') {
        str++;
    }

    if (*str < '0' || *str > '9') {
        return -1; // No valid digits
    }

    uint32_t acc = 0;
    while (*str >= '0' && *str <= '9') {
        uint32_t digit = (uint32_t)(*str - '0');
        // Prevent 32-bit unsigned overflow
        if (acc > (0xFFFFFFFFU - digit) / 10U) {
            return -3; // Overflow error
        }
        acc = acc * 10U + digit;
        str++;
    }

    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    if (*str != '\0') {
        return -1; // Extra trailing characters
    }

    *out_val = acc;
    return 0;
}

unsigned long strtoul(const char* nptr, char** endptr, int base) {
    if (!nptr) return 0;
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || *nptr == '\r') nptr++;

    int sign = 1;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') { nptr++; }

    if (base == 0) {
        if (*nptr == '0') {
            if (nptr[1] == 'x' || nptr[1] == 'X') { base = 16; nptr += 2; }
            else { base = 8; nptr++; }
        } else {
            base = 10;
        }
    } else if (base == 16 && nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
        nptr += 2;
    }

    unsigned long acc = 0;
    int any = 0;
    while (1) {
        int c = *nptr;
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;

        if (digit >= base) break;
        if (acc > (~0UL - digit) / base) {
            acc = ~0UL;
        } else {
            acc = acc * base + digit;
        }
        any = 1;
        nptr++;
    }

    if (endptr) *endptr = (char*)(any ? nptr : (nptr - 1));
    return (sign == -1) ? (unsigned long)(-(long)acc) : acc;
}

long strtol(const char* nptr, char** endptr, int base) {
    return (long)strtoul(nptr, endptr, base);
}

double atof(const char* nptr) {
    if (!nptr) return 0.0;
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' ||
           *nptr == '\r' || *nptr == '\f' || *nptr == '\v') {
        nptr++;
    }
    double sign = 1.0;
    if (*nptr == '-') { sign = -1.0; nptr++; }
    else if (*nptr == '+') { nptr++; }

    double val = 0.0;
    while (*nptr >= '0' && *nptr <= '9') {
        val = val * 10.0 + (*nptr - '0');
        nptr++;
    }
    if (*nptr == '.') {
        nptr++;
        double frac = 0.1;
        while (*nptr >= '0' && *nptr <= '9') {
            val += (*nptr - '0') * frac;
            frac *= 0.1;
            nptr++;
        }
    }
    return val * sign;
}

int system(const char* command) {
    (void)command;
    return -1;
}

char* itoa(int value, char* str, int base) {
    if (!str || base < 2 || base > 36) return NULL;

    char* ptr = str;
    int is_negative = 0;
    unsigned int uval;

    if (value < 0 && base == 10) {
        is_negative = 1;
        uval = (unsigned int)(-value);
    } else {
        uval = (unsigned int)value;
    }

    if (uval == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }

    const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    char* start = ptr;

    while (uval > 0) {
        *ptr++ = digits[uval % (unsigned int)base];
        uval /= (unsigned int)base;
    }

    if (is_negative) {
        *ptr++ = '-';
    }

    *ptr = '\0';

    /* Reverse string in place */
    char* end = ptr - 1;
    while (start < end) {
        char tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }

    return str;
}

int abs(int j) {
    return (j < 0) ? -j : j;
}

long labs(long j) {
    return (j < 0) ? -j : j;
}

char* getenv(const char* name) {
    if (!name) return NULL;
    if (strcmp(name, "PATH") == 0) return "/bin";
    if (strcmp(name, "USER") == 0) return "seld";
    if (strcmp(name, "TERM") == 0) return "xterm";
    if (strcmp(name, "SHELL") == 0) return "/bin/sh";
    return NULL;
}

int errno = 0;

double fabs(double x) {
    return (x < 0.0) ? -x : x;
}

int toupper(int c) {
    return (c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c;
}

int tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isprint(int c) {
    return c >= 32 && c < 127;
}

