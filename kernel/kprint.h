#ifndef __KPRINT__
#define __KPRINT__
#include <stdint.h>

void putchar(char c);
void print_string(char *c);
void printf(char *fmt, ...);
void print_addr(uint32_t addr);
void printint(int32_t num, uint8_t base);
void error(char *str);
void panic(char *str) __attribute__((noreturn));
int strcmp(const char *a, const char *b);
uint32_t strlen(char *str);

extern int hang; // set once panicking: printf stops taking print_lock

#endif