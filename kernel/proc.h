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

// Per-process trapframe that saves user processes register when CPU transitions into supervisor mode.
// Offsets are hard-coded in usertrap.S / usertrapret.S: keep them in sync.
struct __attribute__((aligned(8))) trapframe_t {
    uint32_t k_sp;      // 0
    uint32_t k_trap;    // 4
    uint32_t k_satp;    // 8
    uint32_t u_trap;    // 12
    uint32_t epc;       // 16
    uint32_t regs[32];  // 20, regs[i] = x{i} at 20 + 4*i (x0 slot unused)
    uint32_t k_hartid;  // 148, restored into tp on trap entry: user code owns tp
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
    void *chan;
    struct spinlock lock;
    struct context context;
    struct proc *parent;
};

extern struct cpu cpus[NCPU];
extern struct proc proctable[NPROC];

int cpu_id();
struct proc *procalloc();
struct cpu *this_cpu();
struct proc *myproc();
void init_proctable();
void print_proctable();
void init_userproc();
void swtch(struct context*, struct context*); // swtch.S
void scheduler();
void forkret();
void sched();
void yield();

void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);
void releaseproc(struct proc *p);

pagetable_t init_userpt(); // also used by kexec

int fork();
int wait(uint32_t addr);
void kexit(int status) __attribute__((noreturn));
#endif