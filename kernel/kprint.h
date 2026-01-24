#ifndef __KPRINT__
#define __KPRINT__
#include <stdint.h>

void putchar(char c);
void print_string(char *c);
void printf(char *fmt, ...);
void printint(int32_t num, uint8_t base);
void print_addr(uint32_t addr);
void printint(int32_t num, uint8_t base);
void error(char *str);

#endif