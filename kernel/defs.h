#ifndef __DEF__
#define __DEF__
#include <stdint.h>

#define NULL (void *)0X0ULL
#define PHYSTOP (0x80000000 + 128*1024*1024) // 128 MB top of RAM
#define PGSIZE 4096 // 4KB

// proc.h
#define NCPU 4
#define NPROC 32

// Memory section addresses, refer to kernel.ld for more info

extern char _stext[];
extern char _etext[];
extern char _trampoline[];
extern char data[];
extern char bss[];
extern char stack_top[];
extern char end[];

#endif