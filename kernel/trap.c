#include <stdint.h>
#include "csr.h"
#include "kprint.h"
#include "mem.h"
#include "defs.h"
#include "proc.h"
#include "trap.h"
#include "syscall.h"
#include "rv.h"

void kerneltrap();  // kerneltrap.S
void usertrap();    // usertrap.S
void usertrapret(); // usertrapret.S

// Initializes trap vector into kerneltrap. A dedicated handler for traps happening in supervisor mode.
void init_trap()
{
    w_stvec((uint32_t)kerneltrap); // write stvec into s-mode trap handler
}

// Handle an interrupt. Returns 1 if it was recognised and handled, 0 otherwise.
// TODO(M1): timer (code 5) and external/PLIC (code 9) interrupts go here.
static int devintr(uint32_t scause)
{
    uint32_t code = scause & 0x7FFFFFFF;
    switch (code) {
        case 1: // supervisor software interrupt: nothing uses it yet, just acknowledge
            w_sip(r_sip() & ~(1 << 1));
            return 1;
        default:
            return 0;
    }
}

// Set satp back to the user pagetable before sret
void utrapret() {
    struct proc *p = myproc();

    // Interrupts must stay off until sret: from here on stvec points at
    // usertrap, which assumes it was entered from user mode.
    interrupt_off();

    w_stvec((uint32_t)usertrap);

    // Set sscratch to trapframe for next user trap
    w_sscratch((uint32_t)p->tf);

    p->tf->k_sp = p->kstack + PAGE_SIZE;
    p->tf->k_hartid = r_tp(); // usertrap.S restores tp from here

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
    __builtin_unreachable();
}


// Handle traps happening in u-mode
void u_trap_handle(uint32_t scause, uint32_t sepc) {
    if ((r_sstatus() & SSTATUS_SPP) != 0)
        panic("u_trap_handle: trap not from u-mode");
    struct proc *p = myproc();
    p->tf->epc = sepc; // save program counter
    if (scause & 0x80000000) {
        if (!devintr(scause)) {
            printf("u_trap_handle: unexpected interrupt scause %p\n", scause);
            panic("u_trap_handle");
        }
    }
    else if (scause == SCAUSE_USER_ECALL) {
        p->tf->epc += 4;
        syscall();  // handle the syscall
    }
    else {
        // Faulting user code: returning would just re-run the faulting
        // instruction forever, so kill the process.
        printf("pid %d (%s): killed, scause %p sepc %p stval %p\n",
               p->pid, p->name, scause, sepc, r_stval());
        kexit(-1);
    }
    utrapret();
}

// Handle traps happening in s-mode/kernelmode
void s_trap_handle(uint32_t scause, uint32_t sepc) {
    // A handler that ends up yielding (M1) may take other traps before we
    // return here, which would clobber these CSRs.
    uint32_t sstatus = r_sstatus();

    if ((sstatus & SSTATUS_SPP) == 0)
        panic("s_trap_handle: trap not from s-mode");
    if (is_interrupt())
        panic("s_trap_handle: interrupts enabled");

    if (scause & 0x80000000) {
        if (!devintr(scause)) {
            printf("unexpected interrupt: scause %p, sepc %p\n", scause, sepc);
            panic("s_trap_handle");
        }
    }
    else {
        // The kernel never expects an exception of its own.
        hang = 1; // don't block on print_lock: this hart may be holding it
        printf("scause: %p\n", scause);
        printf("sepc:   %p\n", sepc);
        printf("stval:  %p (Bad Address)\n", r_stval());
        panic("unhandled exception in kernel");
    }

    w_sepc(sepc);
    w_sstatus(sstatus);
}
