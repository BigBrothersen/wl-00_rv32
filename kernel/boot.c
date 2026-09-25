#include "csr.h"
#include "kprint.h"
// #include "trap.h"

void main();

void boot()
{
    
    unsigned long mstatus = r_mstatus();
    mstatus &= ~MSTATUS_MPP_MASK;           // clear MPP (bits 12:11)
    mstatus |=  MSTATUS_MPP_S;
    w_mstatus(mstatus);
    w_mideleg(0xffff);
    w_medeleg(0xffff);
    w_satp(0);
    uint32_t sie = r_sie();
    w_sie(sie | SIE_SEIE | SIE_STIE | SIE_SSIE);
    w_mepc((uint32_t)main); // Direct to main.c
    w_pmpaddr0(0xFFFFFFFF);  // TOR top = 0xFFFFFFFF << 2: covers the whole 34-bit physical space
    w_pmpcfg0(0xF);          // A=TOR, RWX permissions for S/U modes
    asm volatile ("mret");
    while(1) {
        print_string("hanging\n");
    };   // hang if reach to this point
}