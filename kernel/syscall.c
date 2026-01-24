#include "proc.h"
#include "kprint.h"
#include "syscall.h"
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

// TODO: implement sys_exit
uint32_t sys_exit(){
    return 0;
}

uint32_t sys_open() {
    return 0;
}

uint32_t sys_wait() {
    return 0;
}

uint32_t sys_exec() {
    return 0;
}

uint32_t sys_fork() {
    return 0; // proc.c
}

uint32_t sys_read() {
    return 0;
}

// TODO: sanity check, argument extraction, call k_write from fs.c
uint32_t sys_write() {
    int fd;
    uint32_t p_buf; // User Virtual Address
    int n;

    // 1. Fetch Arguments
    argint(0, &fd);
    argaddr(1, &p_buf); // Fetches a0, a1, a2
    argint(2, &n);

    // 2. Safety Checks
    if (fd != 1) return -1; // Only stdout supported for now
    if (n < 0 || n > 1024) return -1; // Safety cap

    // 3. Allocate Kernel Buffer
    char kbuf[128]; 
    
    // Simple loop for large writes
    int wrote = 0;
    while (wrote < n) {
        int chunk = n - wrote;
        if (chunk > 127) chunk = 127; // Copy in small chunks

        // 4. The Magic: Copy from User to Kernel
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
    [SYS_OPEN]      sys_open,
    [SYS_GETPID]    sys_getpid,
};


// Determine the type of syscall from a7 register
void syscall() {
    struct proc *p = this_cpu()->proc;
    int syscall_num = p->tf->regs[17]; // reference a7
    // printf("syscall_num a0 is %d\n", syscall_num);

    if (syscall <= 0 || syscall_num > SYS_GETPID) {
        printf("Error: Invalid syscall %d\n", syscall_num);
        p->tf->regs[10] = -1; // set a0 as -1 for return value
        return;
    }
    p->tf->regs[10] = syscalls[syscall_num](); // execute and store return value in a0
}