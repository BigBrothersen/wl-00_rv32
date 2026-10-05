#ifndef __DEF__
#define __DEF__
#include <stdint.h>

#define NULL ((void *)0)
#define PHYSTOP (0x80000000 + 128*1024*1024) // 128 MB top of RAM
#define PGSIZE 4096 // 4KB

// proc.h
#define NCPU 4  // keep in sync with boot_asm.S and the stack reservation in kernel.ld
#define NPROC 32

// TIMER
#define TIMEBASE_HZ 10000000 // rate of the time CSR: 10 MHz on QEMU virt
#define TICK_HZ 100
#define TIMER_INTERVAL (TIMEBASE_HZ / TICK_HZ)  // every second 100 ticks

// Memory section addresses, refer to kernel.ld for more info

extern char _stext[];
extern char _etext[];
extern char _trampoline[];
extern char data[];
extern char bss[];
extern char stack_top[];
extern char end[];

#endif