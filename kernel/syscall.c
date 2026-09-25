#include "proc.h"
#include "kprint.h"
#include "syscall.h"
#include "exec.h"
// #include "fd.h"

uint32_t argraw(int n) {
    struct proc *p = this_cpu()->proc;
    switch(n) {
        case 0:
            return p->tf->regs[10];
        case 1:
            return p->tf->regs[11];
        case 2:
            return p->tf->regs[12];
        case 3:
            return p->tf->regs[13];
        case 4:
            return p->tf->regs[14];
        case 5:
            return p->tf->regs[15];
    }
    error("argraw");
    return -1;
}

// Retrieves argument
void argaddr(int n, uint32_t *ip) {
    *ip = argraw(n);
}

void argint(int n, int *ip) {
    *ip = argraw(n);
}

uint32_t sys_exit(){
    int status;
    argint(0, &status);

    struct proc *p = this_cpu()->proc;

    // Reparent any children we leave behind to pid 1, if it still exists
    // and isn't us. Otherwise they're simply orphaned (no init yet).
    struct proc *reaper = NULL;
    for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
        if (q->pid == 1 && q != p) {
            reaper = q;
            break;
        }
    }
    for (struct proc *q = proctable; q < &proctable[NPROC]; q++) {
        if (q->parent == p) {
            lock(&q->lock);
            q->parent = reaper;
            unlock(&q->lock);
        }
    }

    // Tear down the address space now: by this point satp is already the
    // kernel page table (usertrap.S switched it before we got here), so
    // p->pt is no longer in use by hardware and is safe to free. The
    // kernel stack and trapframe stay alive - we're still running on the
    // kstack - until a parent reaps this proc via wait().
    uvmfree(p->pt, p->sz);
    p->pt = NULL;

    lock(&p->lock);
    p->xstate = status;
    p->state = ZOMBIE;
    sched(); // never returns
    error("sys_exit: zombie returned");
    return 0;
}

uint32_t sys_open() {
    return 0;
}

uint32_t sys_wait() {
    uint32_t addr;
    argaddr(0, &addr);
    return wait(addr);
}

// Copy a NUL-terminated string of at most max bytes (including the NUL)
// from user space. Returns its length, or -1 on a fault or if too long.
static int fetchstr(uint32_t uaddr, char *buf, int max)
{
    pagetable_t pt = this_cpu()->proc->pt;
    for (int i = 0; i < max; i++) {
        if (copyin(pt, &buf[i], uaddr + i, 1) < 0)
            return -1;
        if (buf[i] == 0)
            return i;
    }
    return -1;
}

// exec(path, argv): path is a NUL-terminated program name, argv a
// NULL-terminated array of string pointers (argv itself may be NULL).
// On success the new program starts with a0 = argc and a1 = argv.
uint32_t sys_exec() {
    char path[MAXPATH], *argv[MAXARG + 1];
    uint32_t upath, uargv, uarg;
    char *strs, *next;
    int argc = 0, ret = -1;

    argaddr(0, &upath);
    argaddr(1, &uargv);

    // 1. Copy path and argv into the kernel while the old image is still
    // mapped. The argument strings share one scratch page.
    if (fetchstr(upath, path, sizeof(path)) < 0)
        return -1;
    if ((strs = kalloc()) == NULL)
        return -1;
    next = strs;
    for (argc = 0; uargv != 0; argc++) {
        if (copyin(this_cpu()->proc->pt, (char *)&uarg, uargv + argc * 4, 4) < 0)
            goto out;
        if (uarg == 0)
            break;
        if (argc >= MAXARG)
            goto out;
        int n = fetchstr(uarg, next, strs + PAGE_SIZE - next);
        if (n < 0)
            goto out;
        argv[argc] = next;
        next += n + 1;
    }
    argv[argc] = NULL;

    // 2-7 live in exec()
    ret = exec(path, argv);

out:
    kfree(strs);
    return ret;
}

uint32_t sys_fork() {
    return fork();
}

uint32_t sys_read() {
    return 0;
}

// TODO: sanity check, argument extraction, call k_write from fs.c
uint32_t sys_write() {
    int fd;
    uint32_t p_buf; // User Virtual Address
    int n;

    // Fetch Arguments
    argint(0, &fd);
    argaddr(1, &p_buf); // Fetches a0, a1, a2
    argint(2, &n);

    // Safety Checks
    if (fd != 1) return -1; // Only stdout supported for now
    if (n < 0 || n > 1024) return -1; // Safety cap

    // Allocate Kernel Buffer
    char kbuf[128]; 
    
    int wrote = 0;
    while (wrote < n) {
        int chunk = n - wrote;
        if (chunk > 127) chunk = 127; // Copy in small chunks

        // copy from User to Kernel
        if (copyin(this_cpu()->proc->pt, kbuf, p_buf + wrote, chunk) == -1) {
            printf("sys_write: fault at %p\n", p_buf + wrote);
            return -1;
        }

        kbuf[chunk] = 0; // Null terminate for printf safety
        printf("%s", kbuf); // Send to UART
        wrote += chunk;
    }
    
    return n;
}

uint32_t sys_brk(){
    return 0;
}

uint32_t sys_kill(){
    return 0;
}

uint32_t sys_getpid() {
    return this_cpu()->proc->pid;
}

static uint32_t (*syscalls[])(void) = {
    [SYS_EXIT]      sys_exit,
    [SYS_OPEN]      sys_open,
    [SYS_WAIT]      sys_wait,
    [SYS_EXEC]      sys_exec,
    [SYS_FORK]      sys_fork,
    [SYS_READ]      sys_read,
    [SYS_WRITE]     sys_write,
    [SYS_BRK]       sys_brk,
    [SYS_KILL]      sys_kill,
    [SYS_GETPID]    sys_getpid,
};


// Determine the type of syscall from a7 register
void syscall() {
    struct proc *p = this_cpu()->proc;
    int syscall_num = p->tf->regs[17]; // reference a7
    // printf("syscall_num a0 is %d\n", syscall_num);

    if (syscall_num <= 0 || syscall_num > SYS_GETPID) {
        printf("Error: Invalid syscall %d\n", syscall_num);
        p->tf->regs[10] = -1; // set a0 as -1 for return value
        return;
    }
    p->tf->regs[10] = syscalls[syscall_num](); // execute and store return value in a0
}