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

    int result = 0;
    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }

    return result * sign;
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

