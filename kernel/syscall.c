#include "proc.h"
#include "kprint.h"
#include "syscall.h"
#include "trap.h"
#include "exec.h"
// #include "fd.h"

uint32_t argraw(int n) {
    struct proc *p = myproc();
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
    kexit(status); // never returns
}

// Unimplemented syscalls fail with -1 rather than pretending to succeed.
uint32_t sys_open() {
    return -1;
}

uint32_t sys_wait() {
    uint32_t addr;
    argaddr(0, &addr);
    return wait(addr);
}

// exec(path, argv): a0 = user address of the program's name, a1 = user address
// of a 0-terminated array of user string addresses (or 0 for no arguments).
// Everything is copied into the kernel first: those addresses only mean
// something through the caller's page table, which kexec frees. Returns -1 to
// the caller on failure; on success the caller is gone and the new program
// starts with a0 = argc, a1 = argv.
uint32_t sys_exec() {
    struct proc *p = myproc();
    char path[MAXPATH], *argv[MAXARG + 1];
    uint32_t upath, uargv, uarg;
    int ret = -1;

    argaddr(0, &upath);
    if (copyinstr(p->pt, path, upath, MAXPATH) < 0) return -1;
    argaddr(1, &uargv);

    // The strings go in a kernel page, packed one after another: too big for
    // the 4 KB kernel stack
    char *buf = kalloc();
    if (buf == NULL) return -1;
    uint32_t used = 0; // bytes of buf filled so far

    if (uargv == 0) {
        argv[0] = 0; // no arguments
    } else {
        int i = 0;
        while (1) {
            if (copyin(p->pt, (char *)&uarg, uargv + 4 * i, 4) < 0) goto out;
            if (uarg == 0) { // end of the list (still fits with MAXARG arguments)
                argv[i] = 0;
                break;
            }
            if (i == MAXARG) goto out; // too many arguments
            argv[i] = buf + used;
            if (copyinstr(p->pt, argv[i], uarg, PAGE_SIZE - used) < 0) goto out;
            used += strlen(argv[i]) + 1;
            i++;
        }
    }
    ret = kexec(path, argv);

out:
    kfree(buf); // on success kexec has already copied the strings to the new stack
    return ret;
}

uint32_t sys_fork() {
    return fork();
}

uint32_t sys_read() {
    return -1;
}

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
        if (copyin(myproc()->pt, kbuf, p_buf + wrote, chunk) == -1) {
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
    return -1;
}

uint32_t sys_kill(){
    return -1;
}

// sleep(n): put the caller to sleep for n clock ticks (n / TICK_HZ seconds),
// using no CPU meanwhile. clockintr() calls wakeup(&ticks) every tick, so the
// loop wakes once per tick, re-checks, and sleeps again until n have passed.
uint32_t sys_sleep(){
    int n;
    argint(0, &n);
    if (n < 0) return -1;

    lock(&tickslock);
    uint32_t start = ticks; // fixed: the moment the sleep began
    while (ticks - start < (uint32_t)n) { // unsigned subtraction: correct across wraparound
        sleep(&ticks, &tickslock);
    }
    unlock(&tickslock);
    return 0;
}

uint32_t sys_getpid() {
    return myproc()->pid;
}
static uint32_t (*syscalls[])(void) = {
    [SYS_EXIT]    = sys_exit,
    [SYS_OPEN]    = sys_open,
    [SYS_WAIT]    = sys_wait,
    [SYS_EXEC]    = sys_exec,
    [SYS_FORK]    = sys_fork,
    [SYS_READ]    = sys_read,
    [SYS_WRITE]   = sys_write,
    [SYS_BRK]     = sys_brk,
    [SYS_KILL]    = sys_kill,
    [SYS_GETPID]  = sys_getpid,
    [SYS_SLEEP]   = sys_sleep,
};


// Determine the type of syscall from a7 register
void syscall() {
    struct proc *p = myproc();
    int syscall_num = p->tf->regs[17]; // reference a7
    // printf("syscall_num a0 is %d\n", syscall_num);

    if (syscall_num <= 0 || syscall_num >= NSYSCALLS || syscalls[syscall_num] == 0) {
        printf("Error: Invalid syscall %d\n", syscall_num);
        p->tf->regs[10] = -1; // set a0 as -1 for return value
        return;
    }
    p->tf->regs[10] = syscalls[syscall_num](); // execute and store return value in a0
}