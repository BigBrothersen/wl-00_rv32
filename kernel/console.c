#include <stdint.h>
#include "console.h"
#include "uart.h"
#include "kprint.h"
#include "spinlock.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

void consputc(int c)
{
    // Backspace
    if (c == BACKSPACE) {
        uart_putc('\b');
        uart_putc(' ');
        uart_putc('\b');
    }
    else {
        if (c == '\n') uart_putc('\r');
        uart_putc(c);
    }
}

// kprint.c
void putchar(char c) {
    consputc((unsigned char)c);
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
        case C('U'): // erase to the back of the line
            while (cons.e != cons.w && cons.buf[(cons.e - 1) % BUF_SIZE] != '\n') {
                consputc(BACKSPACE);
                cons.e--;
            }
            break;
        case '\x7f': // delete
            if (cons.e != cons.w) { 
                cons.e--;
                consputc(BACKSPACE);
            }
            break;
        case C('H'):    // backspace
            if (cons.e != cons.w) { 
                cons.e--;
                consputc(BACKSPACE);
            }
            break;
        default:
            if (c == 0 || (cons.e - cons.r >= BUF_SIZE)) break; // if null or full ignore input
            if (c == '\r') {
                c = '\n';
            }
            putchar(c);
            cons.buf[cons.e % BUF_SIZE] = c;
            cons.e++;
            if (c == '\n' || c == C('D') || cons.e-cons.r == BUF_SIZE) {
                cons.w = cons.e;
                wakeup(&cons.r);
            }
            break;
    }
    unlock(&cons.lock);
}

int console_read(uint32_t dst, int n) {
    uint32_t target = n;
    int c;
    char cbuf;
    lock(&cons.lock);
    while (n > 0) {
        // wait until interrupt has put input into buffer
        while (cons.r == cons.w) {
            if (myproc()->killed) {
                unlock(&cons.lock);
                return -1;
            }
            sleep(&cons.r, &cons.lock);
        }

        c = cons.buf[cons.r % BUF_SIZE];
        cons.r++;
        
        if (c == C('D')) {
            if (n < target) cons.r--;
            break;
        }

        cbuf = c;
        if (copyout(myproc()->pt, dst, &cbuf, 1) < 0) break;

        dst++;
        n--;
        
        if (c == '\n') break; // whole line is delivered
    }
    unlock(&cons.lock);
    return target - n;
}