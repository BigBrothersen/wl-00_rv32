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
// TODO(M1 step 6): add a global spinlock wait_lock.
//  What it protects: every proc's parent pointer, and the "go to sleep in
//  wait() / wake the parent in kexit()" handshake.
//  Rule: write q->parent only while holding BOTH wait_lock and q->lock
//  (then a reader holding either one sees a consistent value).
//  Lock order, always: wait_lock -> p->lock -> mem_lock / print_lock.
//  Never take wait_lock while already holding any p->lock: that is the
//  reverse order and can deadlock against a hart doing it the right way.
struct spinlock wait_lock;

int cpu_id() {
    int id = r_tp();
    return id; 
}


// Must be called with interrupts disabled, or the caller could be moved to
// another CPU between reading tp and using the result.
// TODO(M1 step 4): concept check, no code change expected. With syscalls
// preemptible, a process can now be moved to another hart at almost any line
// of syscall code. Any this_cpu() / cpu_id() / r_tp() result used with
// interrupts ON could name the hart you were on a moment ago.
//  - Go through every caller (grep this_cpu, cpu_id, r_tp) and convince
//    yourself each one runs with interrupts off: inside a held lock, in
//    interrupt context, or after interrupt_off().
//  - Then answer: why is myproc() safe with interrupts on, even though it
//    calls this_cpu() inside?
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
    // TODO(M1 step 6): initialise wait_lock here (hart 0, once, like pid_lock).
    init_lock(&wait_lock, "wait");
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
    // TODO(M1 step 5): also reset the new channel field to 0, so a reused
    // slot never starts with a stale channel.
    p->chan = NULL;
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
                // TODO(M1 step 6): no code change, concept check. This reads
                // p->parent holding only p->lock, not wait_lock. Why is that
                // still safe once the step 6 rule is in place? (Who writes
                // parent, and which locks must they hold?) Also: why must the
                // scheduler NOT take wait_lock here? (Hint: lock order.)
                if (p->state == ZOMBIE && p->parent == NULL)
                    releaseproc(p);
            }
            // release lock (taken by the process in sched())
            unlock(&p->lock);
        }
        // Nothing was runnable: sleep until the next interrupt. Each hart's timer
        // wakes it every tick, and the pending interrupt is then taken in the
        // interrupt_on() window at the top of the loop.
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

// Give up the CPU for one scheduling round: mark ourselves READY and switch to
// the scheduler. Used for preemption on timer ticks.
// p->lock is taken before setting READY so no other hart can pick this process
// up while we are still running on its kernel stack; the scheduler releases it
// once swtch() has left that stack.
void yield()
{
    struct proc *p = myproc();
    lock(&p->lock);
    p->state = READY;
    sched();
    unlock(&p->lock);
}

// TODO(M1 step 5): write sleep(void *chan, struct spinlock *lk).
//  Purpose: block the current process until someone calls wakeup(chan),
//  using no CPU meanwhile (unlike yield(), which stays READY).
//  Contract: the caller holds lk, the lock that protects the condition it is
//  waiting for (e.g. tickslock for "ticks advanced"). sleep() returns with lk
//  held again. Callers always use it in a loop, re-checking the condition:
//      lock(lk); while (!condition) sleep(chan, lk); ... unlock(lk);
//  Procedure:
//   1. Get the current process.
//   2. Take p->lock, and only THEN release lk. This order is the whole trick:
//      wakeup() needs p->lock to look at us, so it cannot run in the gap
//      between "caller checked the condition" and "we are marked WAITING".
//      Swap the order and a wakeup in that gap is lost: we sleep forever.
//   3. Record the channel, set state to WAITING, and call sched().
//      (sched() demands exactly one lock held, which is now p->lock.)
//   4. When we run again (someone woke us and the scheduler picked us):
//      clear the channel, release p->lock, then re-take lk before returning.
//  Think about: why must callers re-check the condition after waking, instead
//  of assuming it is now true? (Several processes may sleep on one channel.)
//  Pitfall: never pass &p->lock itself as lk.
void sleep(void *chan, struct spinlock *lk) {
    struct proc *p = myproc();
    lock(&p->lock);
    unlock(lk);
    p->chan = chan;
    p->state = WAITING;
    sched();
    p->chan = NULL;
    unlock(&p->lock);
    lock(lk);
}

// TODO(M1 step 5): write wakeup(void *chan).
//  Purpose: make every process sleeping on chan runnable again.
//  Procedure: go through the whole proctable; for each process, take its
//  lock, and if it is WAITING on this chan, set it to READY; release the lock.
//  Notes:
//   - Skip the calling process itself: it is not asleep, and if the caller
//     happened to hold its own p->lock, locking it again would panic.
//   - Waking a process that nobody waits for, or calling wakeup() when no one
//     sleeps on chan, is harmless; it just finds nothing to do.
//   - The caller normally holds the condition's lock (lk) while calling this,
//     having just changed the condition. Lock order is then always
//     lk -> p->lock, in both sleep() and wakeup(), so they cannot deadlock.
void wakeup(void *chan) {
    for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
        lock(&q->lock);
        if (q->chan == chan && q->state == WAITING) {
            q->state = READY;
        }
        unlock(&q->lock);
    }
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
    // TODO(M1 step 6): follow the parent rule here too: hold wait_lock as
    // well as np->lock while setting np->parent (wait_lock first, then
    // np->lock). Setting state = READY can stay under np->lock alone.
    lock(&wait_lock);
    lock(&np->lock);
    np->parent = p;
    np->state = READY;
    unlock(&np->lock);
    unlock(&wait_lock);

    return pid;
}

// Waits for any child to become a zombie, reaps it, and returns its pid.
// If addr is non-zero, the child's exit status is copied out to that user
// address. Returns -1 if the caller has no children at all.
int wait(uint32_t addr)
{
    struct proc *p = myproc();

    // TODO(M1 step 6): sleep instead of polling with yield().
    //  Today a waiting parent stays READY and gets scheduled every round just
    //  to re-scan and yield again: it burns CPU doing nothing. Instead:
    //  1. Take wait_lock once, before the for (;;) loop. Holding it makes the
    //     q->parent checks reliable and is the lk that sleep() needs.
    //  2. Keep the scan as it is (q->lock around the ZOMBIE check and reaping).
    //  3. Every return path must release wait_lock first: the reaped-a-child
    //     return, the copyout-failed return, and the no-children return.
    //  4. Replace yield() with sleep(p, &wait_lock). The channel is the
    //     parent's own proc address: that is what kexit() will wake.
    //  Think about: the condition here is "one of my children is a ZOMBIE".
    //  Which lock protects it, so that a child cannot become a zombie and
    //  call wakeup() in the gap between our scan and our sleep()?
    lock(&wait_lock);
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
                    unlock(&wait_lock);
                    return -1;
                }

                releaseproc(q);
                unlock(&q->lock);
                unlock(&wait_lock);
                return pid;
            }
            unlock(&q->lock);
        }

        if (!have_children) {
            unlock(&wait_lock);
            return -1;
        }

        // No zombie child yet: sleep until a child's kexit() wakes us.
        // sleep() releases wait_lock while asleep and re-takes it on return.
        sleep(p, &wait_lock);
    }
}

// Terminate the current process. It stays a ZOMBIE (holding its kstack and
// trapframe, since we are still running on them) until wait() or the
// scheduler reaps it.
void kexit(int status)
{
    struct proc *p = myproc();

    // TODO(M1 step 6): wake the parent instead of letting it poll.
    //  New order of work in kexit():
    //  1. Free the address space (uvmfree) FIRST, before taking any lock: it
    //     needs no lock and can take a while. Move that block up here.
    //  2. Take wait_lock. Do the reparenting loops (unchanged) while holding
    //     it, so the parent writes follow the rule (wait_lock + q->lock).
    //  3. If you handed any child that is already a ZOMBIE to the reaper
    //     (pid 1), wakeup(reaper): it may be asleep in wait() and must reap it.
    //  4. If we have a parent, wakeup(p->parent): it may be asleep in wait().
    //  5. lock(&p->lock), set xstate and ZOMBIE (as today).
    //  6. Release wait_lock, then sched().
    //  Think about:
    //  - Step 4 wakes the parent BEFORE we are a ZOMBIE. Why is that fine?
    //    (What must the parent take before it can re-scan, and when do we
    //    release it?)
    //  - Why must wait_lock be released before sched()? (What does sched()
    //    check about how many locks are held?)

    // Tear down the address space first; it needs no lock. By this point satp
    // is already the kernel page table (usertrap.S switched it before we got
    // here), so p->pt is no longer in use by hardware and is safe to free.
    uvmfree(p->pt, p->sz);
    p->pt = NULL;

    lock(&wait_lock);

    // Reparent any children we leave behind to pid 1, if it still exists
    // and isn't us. Otherwise they're simply orphaned (no init yet).
    struct proc *reaper = NULL;
    for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
        if (q->pid == 1 && q != p && q->state != ZOMBIE) {
            reaper = q;
            break;
        }
    }
    int gave_zombie = 0; // handed the reaper a child that is already a zombie
    for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
        if (q->parent == p) {
            lock(&q->lock);
            q->parent = reaper;
            if (q->state == ZOMBIE) {
                // A zombie orphan will never be scheduled again, so the
                // scheduler can't reap it: do it now.
                if (reaper == NULL)
                    releaseproc(q);
                else
                    gave_zombie = 1;
            }
            unlock(&q->lock);
        }
    }
    // The reaper may be asleep in wait(): it must come and reap that zombie.
    if (gave_zombie)
        wakeup(reaper);

    // Our parent may be asleep in wait(). Waking it before we are a ZOMBIE is
    // fine: it can't re-scan until it gets wait_lock, which we hold until
    // after the state change below.
    if (p->parent)
        wakeup(p->parent);

    lock(&p->lock);
    p->xstate = status;
    p->state = ZOMBIE;
    unlock(&wait_lock); // sched() requires p->lock to be the only lock held
    sched(); // never returns
    panic("kexit: zombie returned");
}
