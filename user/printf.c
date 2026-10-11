// printf for user programs, built on write(1, ...). Supports %d %x %p %s %c %%.
// Only 32-bit arithmetic: 64-bit division would need libgcc, which isn't linked.
#include <stdarg.h>
#include "user.h"

// One syscall per character: slow but simple
static void putc(char c) {
    write(1, &c, 1);
}

static void printint(int x, int base, int sign) {
    char buf[12]; // 10 digits, a sign, one spare
    int i = 0;
    unsigned int u;

    if (sign && x < 0) {
        u = -(unsigned int)x; // as unsigned: -x overflows for -2147483648
    } else {
        u = (unsigned int)x;
        sign = 0;
    }
    // Digits come out lowest first; do-while so 0 still prints one digit
    do {
        buf[i++] = "0123456789abcdef"[u % base];
        u /= base;
    } while (u != 0);
    if (sign)
        buf[i++] = '-';
    while (--i >= 0) // print them back in the right order
        putc(buf[i]);
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (; *fmt != '\0'; fmt++) {
        if (*fmt != '%') {
            putc(*fmt);
            continue;
        }
        fmt++; // the character after '%' says how to print the next argument
        if (*fmt == '\0')
            break;
        switch (*fmt) {
        case 'd':
            printint(va_arg(ap, int), 10, 1);
            break;
        case 'x':
            printint(va_arg(ap, int), 16, 0);
            break;
        case 'p':
            putc('0');
            putc('x');
            printint(va_arg(ap, int), 16, 0);
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (s == 0)
                s = "(null)";
            while (*s != '\0')
                putc(*s++);
            break;
        }
        case 'c':
            putc(va_arg(ap, int)); // chars are passed as int
            break;
        case '%':
            putc('%');
            break;
        default: // unknown: print it as written
            putc('%');
            putc(*fmt);
            break;
        }
    }
    va_end(ap);
}
