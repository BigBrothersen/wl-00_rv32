#ifndef __PROC__
#define __PROC__

#include <stdint.h>
#include "mem.h"
#include "vm.h"
#include "spinlock.h"
#include "defs.h"



enum procstate { UNUSED, NEW, READY, RUNNING, WAITING, ZOMBIE };


struct context {
    uint32_t ra;
    uint32_t sp;
    // Callee-saved
    uint32_t s0; uint32_t s1; uint32_t s2; uint32_t s3;
    uint32_t s4; uint32_t s5; uint32_t s6; uint32_t s7;
    uint32_t s8; uint32_t s9; uint32_t s10; uint32_t s11;
};

struct cpu {
    int id;         
    int depth;      // Depth of push off
    int intena;     // Whether interrupt was on before push_off()

    struct proc *proc; // What process is this cpu running?
    struct context context;
};

// global trapframe variable that saves user processes register when CPU transitions into supervisor mode
__attribute__((aligned(8))) struct trapframe_t {
    uint32_t k_sp;      // 0
    uint32_t k_trap;    // 4
    uint32_t k_satp;    // 8
    uint32_t u_trap;    // 12
    uint32_t epc;       // 16
    uint32_t regs[32];  // 20, x0 is omitted
};

struct proc {
    int pid;
    enum procstate state;        // Process state
    char name[16]; // name of process
    pagetable_t pt; // page table of process
    uint32_t kstack; // top of the kstack, s-mode region of the process
    uint32_t sz; // size of process
    struct trapframe_t *tf; // trapframe of process
    int xstate; // exit status, valid once state == ZOMBIE

    struct spinlock lock;
    struct context context;

    struct proc *parent;
};

extern struct cpu cpus[NCPU];
extern struct proc proctable[NPROC];

struct proc *procalloc();
struct cpu *this_cpu();
void init_proctable();
void print_proctable();
void init_userproc();
void swtch(struct context*, struct context*); // swtch.S
void scheduler();
void forkret();
void sched();
void yield();
void releaseproc(struct proc *p);

int fork();
int wait(uint32_t addr);
#endif