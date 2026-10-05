#include <stdint.h>
#include "spinlock.h"
#include "kprint.h"
#include "rv.h"
#include "proc.h"
#include "csr.h"

// C Wrapper for amoswap in RISC-V
uint32_t atomic_swap(uint32_t *addr, uint32_t new_val) {
    uint32_t old_val;
    asm volatile (
        "amoswap.w.aq %0, %1, (%2)"
        : "=r" (old_val)
        : "r" (new_val),
          "r" (addr)
        : "memory"
    );
    return old_val;
}

void init_lock(struct spinlock *lk, char *name) {
    lk->lock = 0;
    lk->name = name;
    lk->cpu = NULL;
}

void push_off() {
    uint32_t sstatus = r_sstatus();
    interrupt_off();
    struct cpu *curr_cpu = this_cpu();
    if (curr_cpu->depth == 0)
        curr_cpu->intena = sstatus & SSTATUS_SIE;
    curr_cpu->depth++;
}

// TODO(M1 step 4): no code change, concept check. Until now every lock()
// happened with interrupts already off, so intena was always 0 and the
// interrupt_on() below never ran. Once syscalls run with interrupts on, a
// lock() inside a syscall records intena = 1, and the final unlock() turns
// interrupts back on. Trace it for a nested case:
//   lock(A); lock(B); unlock(B); unlock(A);
// At which call do interrupts go off, and at which do they come back on?
// Why would turning them on at unlock(B) be a bug?
void pop_off() {
    struct cpu *curr_cpu = this_cpu();
    if (is_interrupt())
        panic("pop_off - interruptable");
    if (curr_cpu->depth < 1)
        panic("push_off depth less than 1");
    curr_cpu->depth--;
    if (curr_cpu->depth == 0 && curr_cpu->intena)
        interrupt_on(); // Restore interrupt
}

int holding(struct spinlock *lk) {
    return lk->lock && (lk->cpu == this_cpu());
}

void lock(struct spinlock *lk) {
    push_off();
    if (holding(lk))
        panic("trying to lock locked spinlock");
    while (atomic_swap((uint32_t *)&lk->lock, 1) != 0){
        ;
    }

    asm volatile("fence" ::: "memory");

    lk->cpu = this_cpu(); // placeholder
}

void unlock(struct spinlock *lk) {
    if (!holding(lk))
        panic("trying to release unlocked spinlock");
    lk->cpu = NULL;
    asm volatile("fence" ::: "memory");
    atomic_swap((uint32_t *)&lk->lock, 0);
    pop_off();
}
