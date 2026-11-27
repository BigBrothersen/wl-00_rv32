#ifndef __CONSOLE__
#define __CONSOLE__
#include <stdint.h>

void consputc(int c);
void putchar(char c);
void init_console();                     // init_lock(cons.lock) — call from hart 0 in main
void console_intr(int c);                 // producer: ONE byte, called from uartintr
int  console_read(uint32_t dst, int n);   // consumer: bytes copied out, 0 on EOF, -1 on fault

#endif