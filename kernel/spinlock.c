#include <stdint.h>
#include "spinlock.h"
#include "kprint.h"
#include "gpr.h"
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

void pop_off() {
    struct cpu *curr_cpu = this_cpu();
    if (is_interrupt())
        error("pop_off - interruptable");
    if (curr_cpu->depth < 1)
        error("push_off depth less than 1");
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
        error("trying to lock locked spinlock");
    while (atomic_swap((uint32_t *)&lk->lock, 1) != 0){
        ;
    }

    asm volatile("fence" ::: "memory");

    lk->cpu = this_cpu(); // placeholder
}

void unlock(struct spinlock *lk) {
    if (!holding(lk))
        error("trying to release unlocked spinlock");
    lk->cpu = NULL;
    asm volatile("fence" ::: "memory");
    atomic_swap((uint32_t *)&lk->lock, 0);
    pop_off();
}
