#include "csr.h"
#include "proc.h"
#include "trap.h"
#include "vm.h"
#include "kprint.h"
#include "spinlock.h"
#include "rv.h"
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
    
    p->pt = (pagetable_t)kalloc();
    if (p->pt == 0) return NULL;
    memset(p->pt, 0, PAGE_SIZE);

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

// Executes the first user mode program in the kernel.
void uvmfirst(struct proc *p, uint32_t sepc, uint32_t sp) {
    lock(&p->lock);
    
    // Map kernel memory
    uint32_t text_len = (uint32_t)_etext - KERNBASE;
    mappages(p->pt, KERNBASE, KERNBASE, text_len, PTE_R | PTE_X | PTE_V);
    mappages(p->pt, (uint32_t)_etext, (uint32_t)_etext, MEM_END - (uint32_t)_etext, PTE_R | PTE_W | PTE_V);
    mappages(p->pt, (uint32_t)_trampoline, (uint32_t)_trampoline, PAGE_SIZE, PTE_R | PTE_X | PTE_V);

    // // Allocate physical memory to user program
    void *user_pa = kalloc();
    memset(user_pa, 0, PAGE_SIZE);
    if (user_init_bin_len > PAGE_SIZE)
        error("initcode too big");
    memmove(user_pa, user_init_bin, user_init_bin_len);

    // uint32_t *program = (uint32_t*)user_pa;
    // program[0] = 0x00700893;   // addi a7, zero, 7 (original, but we change it)
    // program[1] = 0x00100513;  
    // program[2] = 0x000025B7;   // lui a1, 0x1 (sets a1 to 0x1000)
    // program[3] = 0x02c00613;   // addi a2, zero, 42 (set a2 to 42)
    // program[4] = 0x00000073;   // ecall
    mappage(p->pt, sepc, (uint32_t)user_pa, PTE_R | PTE_X | PTE_U | PTE_V);

    // // Debug
    // va2pa(p->pt, sepc);
    // va2pa(p->pt, sepc+4);
    // printf("value of pa: %p\n", find_pa(p->pt, sepc));
    // printf("value of pa: %p\n", find_pa(p->pt, sepc+4));

    // // Map user stack
    void *stack_pa = kalloc();
    memset(stack_pa, 0, PAGE_SIZE);
    mappage(p->pt, USER_STACK_TOP - PAGE_SIZE, (uint32_t)stack_pa, PTE_R | PTE_W | PTE_U | PTE_V);

    // Load trapframe data to process
    p->tf->epc = sepc;
    p->tf->k_sp = p->kstack + PAGE_SIZE;

    // uint32_t root_pa = (uint32_t)kptable;
    // uint32_t root_ppn = (root_pa >> 12) & 0x003FFFFF;  // 22 bits only
    // uint32_t satp_val = (1 << 31) | root_ppn;  // MODE=1 (Sv32)
    p->tf->k_satp = (uint32_t)MAKE_SATP((uint32_t)kptable);
    p->tf->k_trap = (uint32_t)kerneltrap;
    p->tf->u_trap = (uint32_t)u_trap_handle;
    
    p->state = READY;
    this_cpu()->proc = p;
    unlock(&p->lock);
}


// TODO: make this at user mode!
// first user process executed
void init_userproc()
{
    struct proc *p;
    // printf("User process init\n");
    p = procalloc();
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
        for (p = proctable; p < &proctable[NPROC]; p++) {
            // acquire lock
            lock(&p->lock);
            if (p->state == READY) {
                p->state = RUNNING;
                c->proc = p;
                // printf("test");
                swtch(&c->context, &p->context);
                // Process is done running
                run = 1;
                c->proc = 0;
                printf("Here");
            }
            // release lock
            unlock(&p->lock);
        }
        if(!run) asm volatile("wfi");
    }
}


void forkret()
{
    struct proc *p = this_cpu()->proc;
    unlock(&p->lock);
    utrapret();
}