#include <stdint.h>
#include "csr.h"
#include "kprint.h"
#include "mem.h"
#include "defs.h"
#include "proc.h"
#include "trap.h"
#include "syscall.h"
#include "spinlock.h"
#include "rv.h"

void kerneltrap();  // kerneltrap.S
void usertrap();    // usertrap.S
void usertrapret(); // usertrapret.S

uint32_t ticks;
struct spinlock tickslock;

// Called once, by hart 0, before any hart enables interrupts
void init_clock() {
    init_lock(&tickslock, "ticks");
    ticks = 0;
}

// Initializes trap vector into kerneltrap. A dedicated handler for traps happening in supervisor mode.
void init_trap()
{
    w_stvec((uint32_t)kerneltrap); // write stvec into s-mode trap handler
}

void clockintr() {
    if(cpu_id() == 0) {
        lock(&tickslock);
        ticks++;
        wakeup(&ticks);
        unlock(&tickslock);
    }
}

static int devintr(uint32_t scause)
{
    uint32_t code = scause & 0x7FFFFFFF;
    switch (code) {
        case 1: // supervisor software interrupt: nothing uses it yet, just acknowledge
            w_sip(r_sip() & ~(1 << 1));
            return 1;
        case 5: // code 5 supervisor timer interrupt
            w_stimecmp64(r_time64() + TIMER_INTERVAL);
            clockintr();
            return 2;
        case 9: // code 9
            return 3;
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
    int dev = 0; // devintr() result; stays 0 for ecalls and faults

    if (scause & 0x80000000) {
        // Only interrupts go to devintr(): exception codes reuse the same
        // numbers (5 is also "load access fault"), so a fault would look like a tick.
        dev = devintr(scause);
        if (!dev) {
            printf("u_trap_handle: unexpected interrupt scause %p\n", scause);
            panic("u_trap_handle");
        }
    }
    else if (scause == SCAUSE_USER_ECALL) {
        p->tf->epc += 4;
        interrupt_on();
        syscall();  // handle the syscall
    }
    else {
        // Faulting user code: returning would just re-run the faulting
        // instruction forever, so kill the process.
        printf("pid %d (%s): killed, scause %p sepc %p stval %p\n",
               p->pid, p->name, scause, sepc, r_stval());
        kexit(-1);
    }
    // Timer tick: give up the CPU so other processes get a turn (preemption).
    // Safe here: the user pc is already saved in p->tf and no locks are held.
    // If we resume on another hart, utrapret() sets everything up for that hart.
    if (dev == 2)
        yield();
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

    struct proc *p = myproc();
    int dev = 0;

    if (scause & 0x80000000) {
        dev = devintr(scause);
        if (!dev) {
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
    if (dev == 2 && p != NULL && p->state == RUNNING)
        yield();

    w_sepc(sepc);
    w_sstatus(sstatus);
}
