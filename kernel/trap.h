#ifndef __TRAP__
#define __TRAP__
#include <stdint.h>

// Functions
void init_trap();
void s_trap_handle(uint32_t scause, uint32_t sepc);
void u_trap_handle(uint32_t scause, uint32_t sepc);
void utrapret();

// Assembly functions
extern void kerneltrap(); // kerneltrap.S
extern void usertrap();   // usertrap.S
extern void usertrapret(); // usertrapret.S


// Define
#define SCAUSE_USER_ECALL 8
#define SCAUSE_SUPERVISOR_ECALL 9
#define SCAUSE_MACHINE_ECALL 11

#endif