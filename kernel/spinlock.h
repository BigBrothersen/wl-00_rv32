#ifndef __SPINLOCK__
#define __SPINLOCK__
#include <stdint.h>

struct spinlock{
    unsigned int lock;
    struct cpu *cpu;
    char *name;
};

uint32_t atomic_swap(uint32_t *addr, uint32_t new_val);
void init_lock(struct spinlock *lk, char *name);
void push_off(void);
void pop_off(void);
int holding(struct spinlock *lk);
void lock(struct spinlock *lk);
void unlock(struct spinlock *lk);


#endif