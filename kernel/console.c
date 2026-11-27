#include <stdint.h>
#include "console.h"
#include "uart.h"
#include "kprint.h"
#include "spinlock.h"

#define C(x)  ((x)-'@')  // Control-x

void putchar(char c)
{
    // Backspace
    if (c == 8) {
        uart_putc('\b');
        uart_putc(' ');
        uart_putc('\b');
    }
    else {
        
        uart_putc(c);
    }
}

#define BUF_SIZE 128
struct {
    struct spinlock lock;
    char buf[BUF_SIZE];
    uint32_t r;
    uint32_t w;
    uint32_t e;
} cons;

void init_console() {
    init_lock(&cons.lock, "cons");
    uart_init();
}

void console_intr(int c) {
    lock(&cons.lock);
    switch(c) {
        case C('U'):
            break;
        case C('B'):    // backspace
            putchar('\b');
            cons.e--;
            break;
        default:
            putchar(c);
            cons.buf[cons.e & BUF_SIZE] = c;
            cons.e++;
            if (c == '\n' || cons.e-cons.r == BUF_SIZE) {
                cons.w = cons.e;
                wakeup(&cons.r);
            }
            break;
    }
    unlock(&cons.lock);
}

int console_read(uint32_t dst, int n) {
    return 0;
}