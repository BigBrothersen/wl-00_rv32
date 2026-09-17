#include "csr.h"
#include "proc.h"
#include "trap.h"
#include "vm.h"
#include "kprint.h"
#include "spinlock.h"
#include "rv.h"
#include "uart.h"
#include "initcode.h"

struct cpu cpus[NCPU];
struct proc proctable[NPROC];
int curr_pid = 1;

struct cpu *this_cpu() {
    int id = r_tp();
    return &cpus[id];
}

// Create process table and define kernel memory region
void init_proctable() {
    struct proc *p;
    uint32_t sp = (uint32_t)(&end)-1;
    for (p = proctable; p < &proctable[NPROC]; p++) {
        p->kstack = sp;
        p->state = UNUSED;   
        sp = sp - PAGE_SIZE;
    }
}

// Initializes and allocates page table for the process
pagetable_t init_userpt()
{
    pagetable_t pt;
    pt = (pagetable_t)ptcreate();
    if (pt == 0)
        return NULL;    
    
    // Map text up to trampoline
    uint32_t text_len = (uint32_t)_trampoline - KERNBASE;
    mappages(pt, KERNBASE, KERNBASE, text_len, PTE_R | PTE_X | PTE_V);
    // Map trampoline
    mappages(pt, (uint32_t)_trampoline, (uint32_t)_trampoline, PAGE_SIZE, PTE_R | PTE_X | PTE_V);
    // Map Kernel Data
    uint32_t data_start = (uint32_t)_trampoline + PAGE_SIZE;
    mappages(pt, data_start, data_start, MEM_END - data_start, PTE_R | PTE_W | PTE_V);
    // Map UART
    mappages(pt, UART_0, UART_0, PAGE_SIZE, PTE_R | PTE_W | PTE_V);

    // uint32_t text_len = (uint32_t)_etext - KERNBASE;
    // mappages(pt, KERNBASE, KERNBASE, text_len, PTE_R | PTE_X | PTE_V);
    // mappages(pt, (uint32_t)_etext, (uint32_t)_etext, MEM_END - (uint32_t)_etext, PTE_R | PTE_W | PTE_V);
    // mappages(pt, (uint32_t)_trampoline, (uint32_t)_trampoline, PAGE_SIZE, PTE_R | PTE_X | PTE_V);
    
    return pt;
}

// Find empty process field and returns the address to the process struct. Only for user process only.
struct proc *procalloc() 
{
    struct proc *p;
    int found = 0;
    for (p = proctable; p < &proctable[NPROC]; p++) {
        if (p->state == UNUSED) {
            found = 1;
            break;
        }
    }
    if (!found)
        return NULL;

    p->state = NEW;
    p->pid = curr_pid++;
    
    void *kstack_pa = kalloc();
    if (kstack_pa == 0) return NULL;
    p->kstack = (uint32_t)kstack_pa; 
    
    p->tf = (struct trapframe_t *)kalloc();
    if (p->tf == 0) return NULL;
    memset(p->tf, 0, PAGE_SIZE);
    
    p->pt = (pagetable_t)init_userpt();
    if (p->pt == 0) return NULL;
    // memset(p->pt, 0, PAGE_SIZE);

    //forkret
    p->context.ra = (uint32_t)forkret;
    p->context.sp = p->kstack + PAGE_SIZE;

    // printf("allocted tf to %p\n", p->tf);
    // printf("user page table addr: %p\n", p->pt);
    return p;
}

// Debug: prints process metadata
void print_proc(struct proc p) 
{
    printf("Process PID: %d\n", p.pid);
    printf("State: %d\n", p.state);
    printf("Kernel Stack: %p\n", (void*)p.kstack);
    printf("Page Table: %p\n", (void*)p.pt);
}

// Debug: prints the process table
void print_proctable()
{
    for (int i = 0; i < NPROC; i++) {
        printf("%d sp %p\n", i, proctable[i].kstack);
  }
}

// Frees everything owned by a proc slot (tf, kstack, and pt if it's still
// around) and hands the slot back to UNUSED. Caller must not be running on
// p's kstack (i.e. this is called by a reaper, never by p itself).
void releaseproc(struct proc *p)
{
    if (p->tf)
        kfree((void *)p->tf);
    if (p->pt)
        uvmfree(p->pt, p->sz);
    if (p->kstack)
        kfree((void *)p->kstack);

    p->pid = 0;
    p->state = UNUSED;
    p->name[0] = 0;
    p->pt = NULL;
    p->kstack = 0;
    p->sz = 0;
    p->tf = NULL;
    p->parent = NULL;
    p->xstate = 0;
    memset(&p->context, 0, sizeof(p->context));
}

// Executes the first user mode program in the kernel.
void uvmfirst(struct proc *p, uint32_t sepc, uint32_t sp) 
{
    lock(&p->lock);

    // Allocate physical memory to user program
    void *user_pa = kalloc();
    memset(user_pa, 0, PAGE_SIZE);
    if (user_init_bin_len > PAGE_SIZE)
        error("initcode too big");
    memmove(user_pa, user_init_bin, user_init_bin_len);

    mappage(p->pt, sepc, (uint32_t)user_pa, PTE_R | PTE_X | PTE_U | PTE_V);

    // // Map user stack
    void *stack_pa = kalloc();
    memset(stack_pa, 0, PAGE_SIZE);
    mappage(p->pt, USER_STACK_TOP - PAGE_SIZE, (uint32_t)stack_pa, PTE_R | PTE_W | PTE_U | PTE_V);

    // Load trapframe data to process
    p->tf->epc = sepc;
    p->tf->regs[2] = sp; // x2 = user sp; the `sp` param was never wired up before
    p->tf->k_sp = p->kstack + PAGE_SIZE;

    // uint32_t root_pa = (uint32_t)kptable;
    // uint32_t root_ppn = (root_pa >> 12) & 0x003FFFFF;  // 22 bits only
    // uint32_t satp_val = (1 << 31) | root_ppn;  // MODE=1 (Sv32)
    p->tf->k_satp = (uint32_t)MAKE_SATP((uint32_t)kptable);
    p->tf->k_trap = (uint32_t)kerneltrap;
    p->tf->u_trap = (uint32_t)u_trap_handle;

    p->sz = 0x1000 + PAGE_SIZE;    
    p->state = READY;
    this_cpu()->proc = p;
    unlock(&p->lock);
}

int uvmcopy(pagetable_t old_pt, pagetable_t new_pt, uint32_t sz) 
{
    pagetable_t pte;
    uint32_t va;

    for (va = 0x1000; va < sz; va += PAGE_SIZE) {
        pte = find_pte(old_pt, va, 0);

        if (!pte || (*pte & PTE_V) == 0) continue;

        uint32_t pa = PTE2PA(*pte);
        uint32_t flags = PTE_FLAGS(*pte);

        void *mem; // Allocate page
        if ((mem = kalloc()) == NULL) {
            // TODO: free all the pages
            error("uvmcopy: kalloc");
            return -1;
        }

        memmove(mem, (void *)pa, PAGE_SIZE);
        if (mappage(new_pt, va, (uint32_t)mem, flags) == NULL) {
            error("uvmcopy: mappage");
            // TODO: free all the pages
            return -1;
        }
    }

    // The user stack lives at USER_STACK_TOP, far above `sz`, so the loop
    // above never reaches it - copy it explicitly or the child ends up
    // with no stack mapped at all.
    uint32_t stack_va = USER_STACK_TOP - PAGE_SIZE;
    pte = find_pte(old_pt, stack_va, 0);
    if (!pte || (*pte & PTE_V) == 0) {
        error("uvmcopy: parent has no stack mapped");
        return -1;
    }

    uint32_t stack_pa = PTE2PA(*pte);
    uint32_t stack_flags = PTE_FLAGS(*pte);

    void *stack_mem;
    if ((stack_mem = kalloc()) == NULL) {
        error("uvmcopy: kalloc (stack)");
        return -1;
    }

    memmove(stack_mem, (void *)stack_pa, PAGE_SIZE);
    if (mappage(new_pt, stack_va, (uint32_t)stack_mem, stack_flags) == NULL) {
        error("uvmcopy: mappage (stack)");
        return -1;
    }

    return 0;
}


// first user process executed
void init_userproc()
{
    struct proc *p;
    // printf("User process init\n");
    p = procalloc();
    // printf("pid %d", p->pid);
    if (!p){
        error("procalloc fail");
        return;
    }
    uvmfirst(p, (uint32_t)0x1000, (uint32_t)USER_STACK_TOP);
}

// CPU continously loop through scheduler when CPU is idle
void scheduler()
{
    struct proc *p;
    struct cpu *c = this_cpu();

    while (1) {
        interrupt_on();
        interrupt_off();
        int run = 0;
        // printf("Looping while\n");
        for (p = proctable; p < &proctable[NPROC]; p++) {
            // acquire lock
            lock(&p->lock);
            // printf("Looping proctable\n");
            if (p->state == READY) {
                printf("Found new process with pid %d\n", p->pid);
                p->state = RUNNING;
                c->proc = p;
                // printf("test");
                swtch(&c->context, &p->context);
                // Process is done running
                run = 1;
                c->proc = 0;
                printf("Process exited looking for another process to run\n");
            }
            // release lock (from sys_exit)
            unlock(&p->lock);
        }
        if(!run) asm volatile("wfi");
    }
}

void sched()
{
    struct cpu *c = this_cpu();
    struct proc *p = this_cpu()->proc;
    if (!holding(&p->lock)) {
        error("sched: no lock");
    }
    if (c->depth != 1) {
        error("sched: multiple lock held");
    }
    if (p->state == RUNNING) {
        error("sched: process still running");
    }
    int intena = c->intena;
    swtch(&p->context, &c->context);
    c->intena = intena;
}

// Give up the CPU for one scheduling round without changing our own state
// beyond READY. Used by wait() to poll for a zombie child since this
// kernel has no sleep()/wakeup() channel mechanism yet.
void yield()
{
    struct proc *p = this_cpu()->proc;
    lock(&p->lock);
    p->state = READY;
    sched();
    unlock(&p->lock);
}

void forkret()
{
    struct proc *p = this_cpu()->proc;
    unlock(&p->lock);
    utrapret();
}

int fork() {
    struct proc *p = this_cpu()->proc;
    struct proc *np = procalloc();
    if (!np) {
        error("sys_fork: procalloc fail");
        // TODO: free process slot
        return -1;
    }
    if (uvmcopy(p->pt, np->pt, p->sz) < 0) {
        // TODO: free process slot (fix release proc)
        releaseproc(np);
        return -1;
    }

    np->sz = p->sz;
    uint32_t child_ksp = np->tf->k_sp;
    *(np->tf) = *(p->tf);
    np->tf->k_sp = child_ksp;
    np->tf->regs[10] = 0;   // set child return value as 0

    lock(&np->lock);
    np->parent = p;
    np->state = READY;
    unlock(&np->lock);

    // printf("old process\n");
    // print_proc(*p);
    // printf("new process\n");
    // print_proc(*np);
    // print_proctable();
    
    p->tf->regs[10] = np->pid;

    return np->pid;
}

// Waits for any child to become a zombie, reaps it, and returns its pid.
// If addr is non-zero, the child's exit status is copied out to that user
// address. Returns -1 if the caller has no children at all.
int wait(uint32_t addr)
{
    struct proc *p = this_cpu()->proc;

    for (;;) {
        int have_children = 0;

        for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
            if (q->parent != p)
                continue;
            have_children = 1;

            lock(&q->lock);
            if (q->state == ZOMBIE) {
                int pid = q->pid;
                int xstate = q->xstate;
                unlock(&q->lock);

                if (addr != 0 && copyout(p->pt, addr, (char *)&xstate, sizeof(xstate)) < 0) {
                    error("wait: copyout failed");
                    return -1;
                }

                releaseproc(q);
                return pid;
            }
            unlock(&q->lock);
        }

        if (!have_children)
            return -1;

        yield();
    }
}