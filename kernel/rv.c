#include "csr.h"

// RISC-V related machine functions

int is_interrupt() {
    return (r_sstatus() & SSTATUS_SIE) != 0;
}

void interrupt_on() {
    w_sstatus(r_sstatus() & SSTATUS_SIE);
}

void interrupt_off() {
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}