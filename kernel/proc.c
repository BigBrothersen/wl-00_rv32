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
struct spinlock pid_lock;

// Must be called with interrupts disabled, or the caller could be moved to
// another CPU between reading tp and using the result.
struct cpu *this_cpu() {
    int id = r_tp();
    return &cpus[id];
}

// The process running on this CPU, or NULL. Safe to call with interrupts on.
struct proc *myproc() {
    push_off();
    struct proc *p = this_cpu()->proc;
    pop_off();
    return p;
}

// Create process table. Kernel stacks are allocated per process in procalloc().
void init_proctable() {
    struct proc *p;
    init_lock(&pid_lock, "pid");
    for (p = proctable; p < &proctable[NPROC]; p++) {
        init_lock(&p->lock, "proc");
        p->kstack = 0;
        p->state = UNUSED;
    }
}

static int allocpid() {
    lock(&pid_lock);
    int pid = curr_pid++;
    unlock(&pid_lock);
    return pid;
}

// Initializes and allocates page table for the process
pagetable_t init_userpt()
{
    pagetable_t pt;
    pt = (pagetable_t)ptcreate();
    if (pt == 0)
        return NULL;

    uint32_t text_len = (uint32_t)_trampoline - KERNBASE;
    uint32_t data_start = (uint32_t)_trampoline + PAGE_SIZE;
    if (mappages(pt, KERNBASE, KERNBASE, text_len, PTE_R | PTE_X | PTE_V) < 0 ||                         // text up to trampoline
        mappages(pt, (uint32_t)_trampoline, (uint32_t)_trampoline, PAGE_SIZE, PTE_R | PTE_X | PTE_V) < 0 || // trampoline
        mappages(pt, data_start, data_start, MEM_END - data_start, PTE_R | PTE_W | PTE_V) < 0 ||           // kernel data + RAM
        mappages(pt, UART_0, UART_0, PAGE_SIZE, PTE_R | PTE_W | PTE_V) < 0) {                              // UART
        freewalk(pt); // only kernel mappings so far: frees the tables, not the pages
        return NULL;
    }

    return pt;
}

// Find empty process field and returns the address to the process struct. Only for user process only.
// The slot comes back in state NEW, which the scheduler ignores.
struct proc *procalloc()
{
    struct proc *p;
    int found = 0;
    for (p = proctable; p < &proctable[NPROC]; p++) {
        lock(&p->lock); // another CPU may be claiming the same slot
        if (p->state == UNUSED) {
            p->state = NEW;
            found = 1;
            unlock(&p->lock);
            break;
        }
        unlock(&p->lock);
    }
    if (!found)
        return NULL;

    p->pid = allocpid();

    void *kstack_pa = kalloc();
    if (kstack_pa == 0) goto fail;
    p->kstack = (uint32_t)kstack_pa;

    p->tf = (struct trapframe_t *)kalloc();
    if (p->tf == 0) goto fail;
    memset(p->tf, 0, PAGE_SIZE);

    p->pt = (pagetable_t)init_userpt();
    if (p->pt == 0) goto fail;

    //forkret
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint32_t)forkret;
    p->context.sp = p->kstack + PAGE_SIZE;

    return p;

fail:
    lock(&p->lock);
    releaseproc(p);
    unlock(&p->lock);
    return NULL;
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
// around) and hands the slot back to UNUSED. Caller must hold p->lock and must
// not be running on p's kstack (i.e. this is called by a reaper, never by p itself).
void releaseproc(struct proc *p)
{
    if (!holding(&p->lock))
        panic("releaseproc: lock not held");
    if (p->tf)
        kfree((void *)p->tf);
    if (p->pt)
        uvmfree(p->pt, p->sz);
    if (p->kstack)
        kfree((void *)p->kstack);

    p->pid = 0;
    p->name[0] = 0;
    p->pt = NULL;
    p->kstack = 0;
    p->sz = 0;
    p->tf = NULL;
    p->parent = NULL;
    p->xstate = 0;
    memset(&p->context, 0, sizeof(p->context));
    p->state = UNUSED; // last: procalloc() may claim the slot as soon as it sees this
}

// Executes the first user mode program in the kernel.
void uvmfirst(struct proc *p, uint32_t sepc, uint32_t sp)
{
    lock(&p->lock);

    // Copy the user program in, one page at a time
    uint32_t va;
    for (va = 0; va < user_init_bin_len; va += PAGE_SIZE) {
        void *user_pa = kalloc();
        if (user_pa == 0)
            panic("uvmfirst: out of memory");
        uint32_t n = user_init_bin_len - va;
        if (n > PAGE_SIZE)
            n = PAGE_SIZE;
        memmove(user_pa, user_init_bin + va, n);
        if (mappage(p->pt, sepc + va, (uint32_t)user_pa, PTE_R | PTE_X | PTE_U | PTE_V) == 0)
            panic("uvmfirst: mappage");
    }

    // // Map user stack
    void *stack_pa = kalloc();
    if (stack_pa == 0)
        panic("uvmfirst: out of memory");
    if (mappage(p->pt, USER_STACK_TOP - PAGE_SIZE, (uint32_t)stack_pa, PTE_R | PTE_W | PTE_U | PTE_V) == 0)
        panic("uvmfirst: mappage");

    // Load trapframe data to process
    p->tf->epc = sepc;
    p->tf->regs[2] = sp; // x2 = user sp
    p->tf->k_sp = p->kstack + PAGE_SIZE;
    p->tf->k_satp = (uint32_t)MAKE_SATP((uint32_t)kptable);
    p->tf->k_trap = (uint32_t)kerneltrap;
    p->tf->u_trap = (uint32_t)u_trap_handle;

    p->sz = sepc + PGROUNDUP(user_init_bin_len); // end va of the user image
    memmove(p->name, "init", 5);
    p->state = READY;
    unlock(&p->lock);
}

int uvmcopy(pagetable_t old_pt, pagetable_t new_pt, uint32_t sz)
{
    pte_t *pte;
    uint32_t va;

    for (va = USER_BASE; va < sz; va += PAGE_SIZE) {
        pte = find_pte(old_pt, va, 0);

        if (!pte || (*pte & PTE_V) == 0) continue;

        uint32_t pa = PTE2PA(*pte);
        uint32_t flags = PTE_FLAGS(*pte);

        void *mem; // Allocate page
        if ((mem = kalloc()) == NULL) {
            // pages mapped so far are freed by the caller's releaseproc()
            error("uvmcopy: kalloc");
            return -1;
        }

        memmove(mem, (void *)pa, PAGE_SIZE);
        if (mappage(new_pt, va, (uint32_t)mem, flags) == NULL) {
            error("uvmcopy: mappage");
            kfree(mem);
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
        kfree(stack_mem);
        return -1;
    }

    return 0;
}


// first user process executed
void init_userproc()
{
    struct proc *p;
    p = procalloc();
    if (!p)
        panic("init_userproc: procalloc fail");
    uvmfirst(p, USER_BASE, (uint32_t)USER_STACK_TOP);
}

// CPU continously loop through scheduler when CPU is idle
void scheduler()
{
    struct proc *p;
    struct cpu *c = this_cpu();

    while (1) {
        // Briefly let pending interrupts in, then run with them off.
        interrupt_on();
        interrupt_off();
        int run = 0;
        for (p = proctable; p < &proctable[NPROC]; p++) {
            // acquire lock
            lock(&p->lock);
            if (p->state == READY) {
                p->state = RUNNING;
                c->proc = p;
                swtch(&c->context, &p->context);
                // Process gave up the CPU (yield or exit)
                run = 1;
                c->proc = 0;
                // Nobody will ever wait() for a zombie without a parent (e.g.
                // init itself, or an orphan with no init to adopt it), so reap
                // it here: we are on the scheduler stack, not its kstack.
                if (p->state == ZOMBIE && p->parent == NULL)
                    releaseproc(p);
            }
            // release lock (taken by the process in sched())
            unlock(&p->lock);
        }
        // TODO(M1): without a timer, an idle hart sleeps here until an interrupt arrives.
        if(!run) asm volatile("wfi");
    }
}

void sched()
{
    struct cpu *c = this_cpu();
    struct proc *p = this_cpu()->proc;
    if (!holding(&p->lock)) {
        panic("sched: no lock");
    }
    if (c->depth != 1) {
        panic("sched: multiple lock held");
    }
    if (p->state == RUNNING) {
        panic("sched: process still running");
    }
    if (is_interrupt()) {
        panic("sched: interruptible");
    }
    int intena = c->intena;
    swtch(&p->context, &c->context);
    this_cpu()->intena = intena; // may resume on a different CPU than it left
}

// Give up the CPU for one scheduling round without changing our own state
// beyond READY. Used by wait() to poll for a zombie child since this
// kernel has no sleep()/wakeup() channel mechanism yet.
void yield()
{
    struct proc *p = myproc();
    lock(&p->lock);
    p->state = READY;
    sched();
    unlock(&p->lock);
}

void forkret()
{
    struct proc *p = myproc();
    unlock(&p->lock); // still held from scheduler()
    utrapret();
}

int fork() {
    struct proc *p = myproc();
    struct proc *np = procalloc();
    if (!np) {
        error("sys_fork: procalloc fail");
        return -1;
    }
    if (uvmcopy(p->pt, np->pt, p->sz) < 0) {
        np->sz = p->sz; // so releaseproc() unmaps what uvmcopy managed to copy
        lock(&np->lock);
        releaseproc(np);
        unlock(&np->lock);
        return -1;
    }

    np->sz = p->sz;
    uint32_t child_ksp = np->tf->k_sp;
    *(np->tf) = *(p->tf);
    np->tf->k_sp = child_ksp;
    np->tf->regs[10] = 0;   // set child return value as 0
    memmove(np->name, p->name, sizeof(p->name));

    int pid = np->pid; // np may run, exit and be reaped once READY
    lock(&np->lock);
    np->parent = p;
    np->state = READY;
    unlock(&np->lock);

    return pid;
}

// Waits for any child to become a zombie, reaps it, and returns its pid.
// If addr is non-zero, the child's exit status is copied out to that user
// address. Returns -1 if the caller has no children at all.
int wait(uint32_t addr)
{
    struct proc *p = myproc();

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

                if (addr != 0 && copyout(p->pt, addr, (char *)&xstate, sizeof(xstate)) < 0) {
                    unlock(&q->lock);
                    return -1;
                }

                releaseproc(q);
                unlock(&q->lock);
                return pid;
            }
            unlock(&q->lock);
        }

        if (!have_children)
            return -1;

        yield();
    }
}

// Terminate the current process. It stays a ZOMBIE (holding its kstack and
// trapframe, since we are still running on them) until wait() or the
// scheduler reaps it.
void kexit(int status)
{
    struct proc *p = myproc();

    // Reparent any children we leave behind to pid 1, if it still exists
    // and isn't us. Otherwise they're simply orphaned (no init yet).
    struct proc *reaper = NULL;
    for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
        if (q->pid == 1 && q != p && q->state != ZOMBIE) {
            reaper = q;
            break;
        }
    }
    for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
        if (q->parent == p) {
            lock(&q->lock);
            q->parent = reaper;
            // A zombie orphan will never be scheduled again, so the
            // scheduler can't reap it: do it now.
            if (reaper == NULL && q->state == ZOMBIE)
                releaseproc(q);
            unlock(&q->lock);
        }
    }

    // Tear down the address space now: by this point satp is already the
    // kernel page table (usertrap.S switched it before we got here), so
    // p->pt is no longer in use by hardware and is safe to free.
    uvmfree(p->pt, p->sz);
    p->pt = NULL;

    lock(&p->lock);
    p->xstate = status;
    p->state = ZOMBIE;
    sched(); // never returns
    panic("kexit: zombie returned");
}
