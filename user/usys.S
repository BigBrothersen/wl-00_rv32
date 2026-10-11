# Syscall stubs. A C call like write(1, buf, n) already has its arguments in
# a0..a2 and expects the result in a0, exactly like the syscall convention,
# so each stub only loads the syscall number into a7. ra survives the ecall:
# the trap restores every register and the kernel changes only a0.
#include "sysnum.h"

#define STUB(name, num) \
    .global name;       \
name:                   \
    li a7, num;         \
    ecall;              \
    ret

STUB(exit, SYS_EXIT)
STUB(fork, SYS_FORK)
STUB(wait, SYS_WAIT)
STUB(exec, SYS_EXEC)
STUB(read, SYS_READ)
STUB(write, SYS_WRITE)
STUB(getpid, SYS_GETPID)
STUB(sleep, SYS_SLEEP)
STUB(kill, SYS_KILL)
