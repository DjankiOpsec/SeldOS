/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Standard Input / Output & Buffered Streams Implementation
 * Formatted Printing: %d, %i, %u, %x, %X, %p, %s, %c, %ld, %lx, %lu
 * Buffered Stream I/O: FILE*, stdin, stdout, stderr, fopen, fclose, fread, fwrite
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

/* Standard streams backing storage */
static FILE _stdin_file  = {
    .fd = STDIN_FILENO,
    .flags = _FILE_READ,
    .mode = _IONBF,
    .buf = {0},
    .buf_pos = 0,
    .buf_size = 0,
    .eof = 0,
    .error = 0,
    .is_static = 1
};

static FILE _stdout_file = {
    .fd = STDOUT_FILENO,
    .flags = _FILE_WRITE,
    .mode = _IOLBF,
    .buf = {0},
    .buf_pos = 0,
    .buf_size = 0,
    .eof = 0,
    .error = 0,
    .is_static = 1
};

static FILE _stderr_file = {
    .fd = STDERR_FILENO,
    .flags = _FILE_WRITE,
    .mode = _IONBF,
    .buf = {0},
    .buf_pos = 0,
    .buf_size = 0,
    .eof = 0,
    .error = 0,
    .is_static = 1
};

FILE* stdin  = &_stdin_file;
FILE* stdout = &_stdout_file;
FILE* stderr = &_stderr_file;

/* -------------------------------------------------------------
 * Stream Flushing & Primitive I/O
 * ------------------------------------------------------------- */

int fflush(FILE* stream) {
    if (!stream) return 0;
    if (stream->flags & (_FILE_WRITE | _FILE_RDWR)) {
        if (stream->buf_pos > 0) {
            ssize_t written = seld_write(stream->fd, stream->buf, (size_t)stream->buf_pos);
            int orig_pos = stream->buf_pos;
            stream->buf_pos = 0;
            if (written < orig_pos) {
                stream->error = 1;
                return EOF;
            }
        }
    }
    return 0;
}

int feof(FILE* stream) {
    return stream ? stream->eof : 1;
}

int ferror(FILE* stream) {
    return stream ? stream->error : 1;
}

int fputc(int c, FILE* stream) {
    if (!stream) return EOF;
    unsigned char ch = (unsigned char)c;

    if (stream->mode == _IONBF) {
        if (seld_write(stream->fd, &ch, 1) != 1) {
            stream->error = 1;
            return EOF;
        }
        return ch;
    }

    stream->buf[stream->buf_pos++] = (char)ch;
    if (stream->buf_pos >= BUFSIZ || (stream->mode == _IOLBF && ch == '\n')) {
        if (fflush(stream) == EOF) {
            return EOF;
        }
    }
    return ch;
}

int fgetc(FILE* stream) {
    if (!stream) return EOF;
    if (stream->eof) return EOF;

    if (stream->mode == _IONBF) {
        char ch;
        ssize_t n = seld_read(stream->fd, &ch, 1);
        if (n <= 0) {
            stream->eof = 1;
            return EOF;
        }
        return (unsigned char)ch;
    }

    if (stream->buf_pos >= stream->buf_size) {
        ssize_t n = seld_read(stream->fd, stream->buf, BUFSIZ);
        if (n <= 0) {
            stream->eof = 1;
            return EOF;
        }
        stream->buf_size = (int)n;
        stream->buf_pos = 0;
    }

    return (unsigned char)stream->buf[stream->buf_pos++];
}

int putchar(int c) {
    return fputc(c, stdout);
}

int getchar(void) {
    return fgetc(stdin);
}

int fputs(const char* s, FILE* stream) {
    if (!s || !stream) return EOF;
    while (*s) {
        if (fputc((unsigned char)*s++, stream) == EOF) {
            return EOF;
        }
    }
    return 0;
}

int puts(const char* s) {
    if (!s) return EOF;
    if (fputs(s, stdout) == EOF) return EOF;
    if (fputc('\n', stdout) == EOF) return EOF;
    return 0;
}

char* fgets(char* s, int size, FILE* stream) {
    if (!s || size <= 0 || !stream) return NULL;
    int idx = 0;
    while (idx < size - 1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (idx == 0) return NULL;
            break;
        }
        s[idx++] = (char)c;
        if (c == '\n') break;
    }
    s[idx] = '\0';
    return s;
}

/* -------------------------------------------------------------
 * File Stream Management (fopen, fclose, fread, fwrite)
 * ------------------------------------------------------------- */

FILE* fopen(const char* filename, const char* mode) {
    if (!filename || !mode) return NULL;

    int flags = 0;
    int perm_flags = 0;
    if (mode[0] == 'r') {
        if (mode[1] == '+') {
            flags = O_RDWR;
            perm_flags = _FILE_RDWR;
        } else {
            flags = O_RDONLY;
            perm_flags = _FILE_READ;
        }
    } else if (mode[0] == 'w') {
        if (mode[1] == '+') {
            flags = O_RDWR | O_CREAT | O_TRUNC;
            perm_flags = _FILE_RDWR;
        } else {
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            perm_flags = _FILE_WRITE;
        }
    } else if (mode[0] == 'a') {
        if (mode[1] == '+') {
            flags = O_RDWR | O_CREAT | O_APPEND;
            perm_flags = _FILE_RDWR;
        } else {
            flags = O_WRONLY | O_CREAT | O_APPEND;
            perm_flags = _FILE_WRITE;
        }
    } else {
        return NULL;
    }

    int fd = open(filename, flags, 0666);
    if (fd < 0) return NULL;

    FILE* fp = (FILE*)malloc(sizeof(FILE));
    if (!fp) {
        close(fd);
        return NULL;
    }

    fp->fd = fd;
    fp->flags = perm_flags;
    fp->mode = _IOFBF;
    fp->buf_pos = 0;
    fp->buf_size = 0;
    fp->eof = 0;
    fp->error = 0;
    fp->is_static = 0;

    return fp;
}

int fclose(FILE* stream) {
    if (!stream) return EOF;
    fflush(stream);
    int res = close(stream->fd);
    if (!stream->is_static) {
        free(stream);
    }
    return (res < 0) ? EOF : 0;
}

int remove(const char* pathname) {
    return unlink(pathname);
}

int rename(const char* oldpath, const char* newpath) {
    (void)oldpath;
    (void)newpath;
    return 0; // SeldFS flat fs stub
}

int fseek(FILE* stream, long offset, int whence) {
    if (!stream) return -1;
    fflush(stream);
    stream->buf_pos = 0;
    stream->buf_size = 0;
    stream->eof = 0;
    off_t res = lseek(stream->fd, (off_t)offset, whence);
    if (res < 0) {
        stream->error = 1;
        return -1;
    }
    return 0;
}

long ftell(FILE* stream) {
    if (!stream) return -1;
    off_t cur = lseek(stream->fd, 0, SEEK_CUR);
    if (cur < 0) return -1;
    // Account for buffered unread bytes
    long actual = (long)cur - (stream->buf_size - stream->buf_pos);
    return actual;
}

void rewind(FILE* stream) {
    if (stream) {
        fseek(stream, 0, SEEK_SET);
        stream->error = 0;
    }
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || size == 0 || nmemb == 0 || !stream) return 0;
    size_t total = size * nmemb;
    unsigned char* p = (unsigned char*)ptr;
    size_t bytes_read = 0;

    // First exhaust internal stream buffer
    while (stream->buf_pos < stream->buf_size && bytes_read < total) {
        p[bytes_read++] = (unsigned char)stream->buf[stream->buf_pos++];
    }

    if (bytes_read == total) {
        return nmemb;
    }

    // Direct read for large chunks
    if (total - bytes_read >= BUFSIZ) {
        size_t direct_read = total - bytes_read;
        ssize_t n = seld_read(stream->fd, p + bytes_read, direct_read);
        if (n <= 0) {
            stream->eof = 1;
            return bytes_read / size;
        }
        bytes_read += (size_t)n;
        if (bytes_read == total) {
            return nmemb;
        }
    }

    // Refill buffer for remaining bytes
    while (bytes_read < total) {
        int c = fgetc(stream);
        if (c == EOF) break;
        p[bytes_read++] = (unsigned char)c;
    }

    return bytes_read / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || size == 0 || nmemb == 0 || !stream) return 0;
    size_t total = size * nmemb;
    const unsigned char* p = (const unsigned char*)ptr;
    size_t bytes_written = 0;

    while (bytes_written < total) {
        if (fputc((int)p[bytes_written], stream) == EOF) break;
        bytes_written++;
    }

    return bytes_written / size;
}

int sscanf(const char* str, const char* format, ...) {
    if (!str || !format) return 0;
    va_list args;
    va_start(args, format);
    int matched = 0;
    const char* s = str;
    const char* f = format;

    while (*f && *s) {
        if (*f == ' ') {
            while (*s == ' ' || *s == '\t' || *s == '\n') s++;
            f++;
            continue;
        }
        if (*f == '%') {
            f++;
            if (*f == 'd' || *f == 'i') {
                while (*s == ' ' || *s == '\t' || *s == '\n') s++;
                int sign = 1;
                if (*s == '-') { sign = -1; s++; }
                else if (*s == '+') { s++; }
                if (*s < '0' || *s > '9') break;
                int val = 0;
                while (*s >= '0' && *s <= '9') {
                    val = val * 10 + (*s - '0');
                    s++;
                }
                int* out = va_arg(args, int*);
                if (out) *out = val * sign;
                matched++;
                f++;
            } else if (*f == 's') {
                while (*s == ' ' || *s == '\t' || *s == '\n') s++;
                char* out = va_arg(args, char*);
                int idx = 0;
                while (*s && *s != ' ' && *s != '\t' && *s != '\n') {
                    if (out) out[idx++] = *s;
                    s++;
                }
                if (out) out[idx] = '\0';
                matched++;
                f++;
            } else {
                break;
            }
        } else {
            if (*f != *s) break;
            f++;
            s++;
        }
    }

    va_end(args);
    return matched;
}

/* -------------------------------------------------------------
 * Formatted Printing Engine (vsnprintf, snprintf, sprintf, printf)
 * ------------------------------------------------------------- */

static void buffer_putc(char** out_ptr, size_t* remain, size_t* total, char c) {
    (*total)++;
    if (*remain > 1) {
        **out_ptr = c;
        (*out_ptr)++;
        (*remain)--;
    }
}

static void buffer_puts(char** out_ptr, size_t* remain, size_t* total, const char* s, int width, int left_align, char pad, int max_len) {
    if (!s) s = "(null)";
    int len = 0;
    while (s[len] && (max_len < 0 || len < max_len)) len++;

    int padding = (width > len) ? (width - len) : 0;
    if (!left_align) {
        while (padding-- > 0) {
            buffer_putc(out_ptr, remain, total, pad);
        }
    }
    for (int i = 0; i < len; i++) {
        buffer_putc(out_ptr, remain, total, s[i]);
    }
    if (left_align) {
        while (padding-- > 0) {
            buffer_putc(out_ptr, remain, total, pad);
        }
    }
}

static void format_number(char** out_ptr, size_t* remain, size_t* total,
                          uint64_t num, int base, int is_signed, int uppercase,
                          int width, int left_align, char pad) {
    char num_buf[32];
    int idx = 0;
    int negative = 0;

    if (is_signed && (int64_t)num < 0) {
        negative = 1;
        num = (uint64_t)(-(int64_t)num);
    }

    if (num == 0) {
        num_buf[idx++] = '0';
    } else {
        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        while (num > 0) {
            num_buf[idx++] = digits[num % (unsigned)base];
            num /= (unsigned)base;
        }
    }

    int len = idx + (negative ? 1 : 0);
    int padding = (width > len) ? (width - len) : 0;

    if (pad == '0' && negative) {
        buffer_putc(out_ptr, remain, total, '-');
        negative = 0;
    }

    if (!left_align) {
        while (padding-- > 0) {
            buffer_putc(out_ptr, remain, total, pad);
        }
    }

    if (negative) {
        buffer_putc(out_ptr, remain, total, '-');
    }

    for (int i = idx - 1; i >= 0; i--) {
        buffer_putc(out_ptr, remain, total, num_buf[i]);
    }

    if (left_align) {
        while (padding-- > 0) {
            buffer_putc(out_ptr, remain, total, ' ');
        }
    }
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    char* out = str;
    size_t remain = size;
    size_t total = 0;

    while (*format) {
        if (*format != '%') {
            buffer_putc(&out, &remain, &total, *format++);
            continue;
        }

        format++; /* Skip '%' */
        if (*format == '%') {
            buffer_putc(&out, &remain, &total, '%');
            format++;
            continue;
        }

        /* Flags */
        int left_align = 0;
        char pad = ' ';
        while (*format == '-' || *format == '0') {
            if (*format == '-') left_align = 1;
            if (*format == '0') pad = '0';
            format++;
        }
        if (left_align) pad = ' ';

        /* Field width */
        int width = 0;
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }

        /* Precision: .<digits> */
        int precision = -1;
        if (*format == '.') {
            format++;
            precision = 0;
            while (*format >= '0' && *format <= '9') {
                precision = precision * 10 + (*format - '0');
                format++;
            }
        }

        /* Length modifiers */
        int is_long = 0;
        while (*format == 'l' || *format == 'z') {
            is_long++;
            format++;
        }

        /* Specifier */
        switch (*format) {
            case 'd':
            case 'i': {
                int64_t val = (is_long > 0) ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int);
                int eff_width = width;
                char eff_pad = pad;
                if (precision >= 0) {
                    if (precision > eff_width) eff_width = precision;
                    eff_pad = '0';
                }
                format_number(&out, &remain, &total, (uint64_t)val, 10, 1, 0, eff_width, left_align, eff_pad);
                break;
            }
            case 'u': {
                uint64_t val = (is_long > 0) ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int);
                int eff_width = width;
                char eff_pad = pad;
                if (precision >= 0) {
                    if (precision > eff_width) eff_width = precision;
                    eff_pad = '0';
                }
                format_number(&out, &remain, &total, val, 10, 0, 0, eff_width, left_align, eff_pad);
                break;
            }
            case 'x': {
                uint64_t val = (is_long > 0) ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int);
                int eff_width = width;
                char eff_pad = pad;
                if (precision >= 0) {
                    if (precision > eff_width) eff_width = precision;
                    eff_pad = '0';
                }
                format_number(&out, &remain, &total, val, 16, 0, 0, eff_width, left_align, eff_pad);
                break;
            }
            case 'X': {
                uint64_t val = (is_long > 0) ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int);
                int eff_width = width;
                char eff_pad = pad;
                if (precision >= 0) {
                    if (precision > eff_width) eff_width = precision;
                    eff_pad = '0';
                }
                format_number(&out, &remain, &total, val, 16, 0, 1, eff_width, left_align, eff_pad);
                break;
            }
            case 'p': {
                uint64_t val = (uint64_t)va_arg(ap, void*);
                buffer_putc(&out, &remain, &total, '0');
                buffer_putc(&out, &remain, &total, 'x');
                format_number(&out, &remain, &total, val, 16, 0, 0, (width > 2) ? (width - 2) : 0, left_align, pad);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                buffer_puts(&out, &remain, &total, s, width, left_align, pad, precision);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                buffer_putc(&out, &remain, &total, c);
                break;
            }
            case 'f': {
                double val = va_arg(ap, double);
                if (val < 0) {
                    buffer_putc(&out, &remain, &total, '-');
                    val = -val;
                }
                int64_t int_part = (int64_t)val;
                format_number(&out, &remain, &total, (uint64_t)int_part, 10, 0, 0, 0, 0, ' ');
                buffer_putc(&out, &remain, &total, '.');
                int prec = (precision >= 0) ? precision : 6;
                double frac = val - (double)int_part;
                for (int i = 0; i < prec; i++) {
                    frac *= 10.0;
                    int digit = (int)frac;
                    if (digit > 9) digit = 9;
                    buffer_putc(&out, &remain, &total, (char)('0' + digit));
                    frac -= (double)digit;
                }
                break;
            }
            default:
                buffer_putc(&out, &remain, &total, *format);
                break;
        }

        if (*format) format++;
    }

    if (size > 0) {
        if (remain > 0) {
            *out = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }

    return (int)total;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int vsprintf(char* str, const char* format, va_list ap) {
    return vsnprintf(str, (size_t)-1, format, ap);
}

int sprintf(char* str, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, (size_t)-1, format, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buf[1024];
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(buf, sizeof(buf), format, ap_copy);
    va_end(ap_copy);

    if (len < (int)sizeof(buf)) {
        fwrite(buf, 1, (size_t)len, stream);
    } else {
        char* big_buf = (char*)malloc((size_t)len + 1);
        if (big_buf) {
            vsnprintf(big_buf, (size_t)len + 1, format, ap);
            fwrite(big_buf, 1, (size_t)len, stream);
            free(big_buf);
        }
    }
    return len;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char* format, va_list ap) {
    return vfprintf(stdout, format, ap);
}

int printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}

/* -------------------------------------------------------------
 * Backward Compatibility Helpers for Kernel / Shell Tests
 * ------------------------------------------------------------- */

void print_hex(uint64_t val) {
    printf("0x%016lx", val);
}

void print_dec(uint64_t val) {
    printf("%lu", val);
}
