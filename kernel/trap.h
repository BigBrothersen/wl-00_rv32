#ifndef __TRAP__
#define __TRAP__
#include <stdint.h>

// Functions
void init_clock();
void init_trap();
void s_trap_handle(uint32_t scause, uint32_t sepc);
void u_trap_handle(uint32_t scause, uint32_t sepc);
void utrapret() __attribute__((noreturn));

// Assembly functions
extern void kerneltrap(); // kerneltrap.S
extern void usertrap();   // usertrap.S
extern void usertrapret(); // usertrapret.S


// Clock
extern uint32_t ticks;
extern struct spinlock tickslock;

// Define
#define SSOFTWARE_INTR 1
#define STIMER_INTR 2

#define SCAUSE_USER_ECALL 8
#define SCAUSE_SUPERVISOR_ECALL 9
#define SCAUSE_MACHINE_ECALL 11

#endif