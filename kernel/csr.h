#ifndef __CSR__
#define __CSR__
#include <stdint.h>

// Machine Mode Status Register (mstatus) bits
#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)    // MPP value for Machine mode
#define MSTATUS_MPP_S (1L << 11)    // MPP value for Supervisor mode
#define MSTATUS_MPP_U (0L << 11)    // MPP value for User mode
#define MSTATUS_MIE (1L << 3)       // machine-mode interrupt enable.

// Supervisor Mode Status Register (sstatus) bits
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

// Supervisor Interrupt Enable (sie) bits
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

// Machine Information Registers
uint32_t r_mvendorid();
uint32_t r_marchid();
uint32_t r_mimpid();
uint32_t r_mhartid();

// Machine Trap Setup
void w_mstatus(uint32_t x);
uint32_t r_mstatus();
void w_misa(uint32_t x);
uint32_t r_misa();
void w_medeleg(uint32_t x);
uint32_t r_medeleg();
void w_mideleg(uint32_t x);
uint32_t r_mideleg();
void w_mie(uint32_t x);
uint32_t r_mie();
void w_mtvec(uint32_t x);
uint32_t r_mtvec();

// Machine Trap Handling
uint32_t r_mscratch();
void w_mscratch(uint32_t x);
uint32_t r_mepc();
void w_mepc(uint32_t x);
uint32_t r_mcause();
uint32_t r_mtval();
uint32_t r_mip();

// Supervisor Trap Setup
void w_sstatus(uint32_t x);
uint32_t r_sstatus();
void w_sie(uint32_t x);
uint32_t r_sie();
void w_stvec(uint32_t x);
uint32_t r_stvec();

// Supervisor Trap Handling
uint32_t r_sscratch();
void w_sscratch(uint32_t x);
uint32_t r_sepc();
void w_sepc(uint32_t x);
uint32_t r_scause();
uint32_t r_stval();
uint32_t r_sip();
void w_sip(uint32_t x);

// Supervisor Protection and Translation
void w_satp(uint32_t x);
uint32_t r_satp();

// Machine Counters/Timers
uint32_t r_mcycle();
uint32_t r_minstret();

// Physical Memory Protection
void w_pmpcfg0(uint32_t x);
uint32_t r_pmpcfg0();
void w_pmpaddr0(uint32_t x);
uint32_t r_pmpaddr0();

void w_tp(uint32_t x);
uint32_t r_tp();

// Additional useful macros
#define CSR_MACHINE 0
#define CSR_SUPERVISOR 1
#define CSR_USER 2

#define TRAP_DIRECT 0
#define TRAP_VECTORED 1

#endif // __CSR__