#include <stdint.h>
#include "csr.h"
#include "kprint.h"
#include "mem.h"
#include "defs.h"
#include "proc.h"
#include "trap.h"
#include "syscall.h"

void kerneltrap();  // kerneltrap.S
void usertrap();    // usertrap.S
void usertrapret(); // usertrapret.S

void main();

// Initializes trap vector into kerneltrap. A dedicated handler for traps happening in supervisor mode.
void init_trap() 
{
    w_stvec((uint32_t)kerneltrap); // write stvec into s-mode trap handler
}

// Set satp back to the user pagetable before sret

void utrapret() {
    struct proc *p = this_cpu()->proc;
    
    w_stvec((uint32_t)usertrap);
    
    // Set sscratch to trapframe for next user trap
    w_sscratch((uint32_t)p->tf);

    p->tf->k_sp = p->kstack + PAGE_SIZE; 

    // Prepare to return to user mode
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP;    // Clear SPP to return to U-mode
    x |= SSTATUS_SPIE;    // Enable interrupts in user mode
    w_sstatus(x);

    // Set return address to user code
    w_sepc(p->tf->epc);
    
    // Switch to user page table
    __asm__ volatile("sfence.vma zero, zero");
    w_satp(MAKE_SATP((uint32_t)p->pt));
    __asm__ volatile("sfence.vma zero, zero");

    // Jump to assembly function to restore user context
    // Pass the trapframe address as parameter in a0
    __asm__ volatile(
        "mv a0, %0\n\t"    // Pass trapframe address as first argument
        "jr %1" 
        : 
        : "r" (p->tf), "r" (usertrapret)
        : "a0"
    );
}


// Handle traps happening in u-mode
void u_trap_handle(uint32_t scause, uint32_t sepc) {
    if ((r_sstatus() & SSTATUS_SPP) != 0)
        error("Trap not from u-mode\n");
    struct proc *p = this_cpu()->proc;
    printf("Handling program for process %d\n", p->pid);
    p->tf->epc = sepc; // save program counter
    if (scause == SCAUSE_USER_ECALL) {
        // printf("TRAP from u-mode: scause %p, sepc %p\n", scause, sepc);
        p->tf->epc += 4;
        syscall();  // handle the syscall 
    }
    else {
        error("usertrap(): unexpected scause");
        // int x = 0;
    }
    utrapret();
}

// Handle traps happening in s-mode/kernelmode TODO
void s_trap_handle(uint32_t scause, uint32_t sepc) {

    int is_interrupt = (scause & 0x80000000);

    if (is_interrupt) {
        uint32_t code = scause & 0x7FFFFFFF;
        printf("Interrupt received: %d\n", code);
        // TEMPORARY: If it's a timer (IRQ 5), clear it so we don't loop forever
        // w_sip(r_sip() & ~(1 << 5)); 
        return; 
    }

    switch (scause) {
        case 0x8:
        case 0x9:
            printf("Syscall/Ecall detected (scause %d) at %p\n", scause, sepc);
            w_sepc(sepc + 4);
            break;
        case 0xb:
            printf("Syscall/Ecall detected (scause %d) at %p\n", scause, sepc);
            w_sepc(sepc + 4);
            break;
        default: {
            printf("KERNEL PANIC! Unhandled Exception.\n");
            printf("scause: %p\n", scause);
            printf("sepc:   %p\n", sepc);
            printf("stval:  %p (Bad Address)\n", r_stval());
            while(1);
        }
    }
}
