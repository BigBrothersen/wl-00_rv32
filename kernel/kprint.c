#include <stdint.h>
#include <stdarg.h>
#include "kprint.h"
#include "uart.h"
#include "spinlock.h"

// TODO: migrate print functions to user space

const char *digits = "0123456789abcdef";

int hang = 0;

struct spinlock print_lock;

uint32_t strlen(char *str)
{
    uint32_t count = 0;
    while (*str != '\0') {
        count++;
    }
    return count;
}

void error(char *str)
{
    printf("error: %s\n", str);
}

void putchar(char c)
{
    *(volatile char*)UART_0 = c;
}

void print_string(char *str)
{
    while(*str){
        putchar(*str++);
    }
}

void print_addr(uint32_t addr) 
{
    print_string("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (addr >> i) & 0xF;
        putchar(digits[(addr >> i) & 0xF]);
    }
}

// TODO: implement printint. will expand to different base and size later
void print_uint(uint32_t num, uint8_t base) 
{
    char nums[64]; // larger buffer for 64-bit numbers
    int i = 0;
    do {
        nums[i++] = digits[num % base];
        num /= base;
    } while (num != 0);

    // print in correct order
    while (i > 0) {
        putchar(nums[--i]);
    }
}

void printint(int32_t num, uint8_t base)
{
    if (!(base == 2 || base == 8 || base == 10 || base == 16)){
        error("printint: invalid base");
        return;
    }
    char nums[16];
    int i;
    int negative;
    negative = 0;
    if (num < 0){
        negative = 1;
        num = -num;
    }
    i = 0;
    do {
        nums[i++] = digits[num % base];
    } while((num /= base) != 0);
    if (negative) {
        nums[i++] = '-';
    }
    while(--i >= 0) {
        putchar(nums[i]);
    }
}

// TODO: implement printf for booting message and debugging
// TODO: add error handling for each format specifier
void printf(char *fmt, ...)
{
    if (!hang)
        lock(&print_lock);
    int terminate = 0;
    va_list args;
    va_start(args, fmt);
    while (*fmt && !terminate){
        if (*fmt == '%'){
            fmt++;
            switch (*fmt) {
                case '\0':{
                    error("printf: format specifier followed by null");
                    terminate = 1;
                    break;
                }
                case 'd':{
                    int num = va_arg(args, int);
                    printint(num, 10);
                    fmt++;
                    break;
                }
                case 'u':{
                    unsigned long long num = va_arg(args, unsigned long long);
                    print_uint(num, 10);
                    fmt++;
                    break;
                }
                case 'x':{
                    int num = va_arg(args, int) ;
                    printint(num, 16);
                    fmt++;
                    break;
                }
                case 'p':{
                    char *ptr = va_arg(args, char *);
                    print_addr((uint32_t)(uintptr_t)ptr);
                    fmt++;
                    break;
                }
                case 's':{
                    char *str = va_arg(args, char*);
                    print_string(str);
                    fmt++;
                    break;
                }
                case 'c':{
                    int c = va_arg(args, int);
                    putchar((char)c);
                    fmt++;
                    break;
                }
                default:{
                    print_string("unknown specifier");
                    terminate = 1;
                    break;
                }
            }
        }
        else {
            putchar(*fmt++);
        }
    }
    va_end(args);

    if (!hang)
        unlock(&print_lock);
}