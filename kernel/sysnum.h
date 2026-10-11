// Syscall numbers: what user programs put in a7. Plain #defines (no C code)
// so the kernel and the user stubs in usys.S, which is assembly, can share them.
// Only ever append new ones, never insert: existing numbers must not shift.
#ifndef __SYSNUM__
#define __SYSNUM__

#define SYS_EXIT 1
#define SYS_OPEN 2
#define SYS_WAIT 3
#define SYS_EXEC 4
#define SYS_FORK 5
#define SYS_READ 6
#define SYS_WRITE 7
#define SYS_BRK 8
#define SYS_KILL 9
#define SYS_GETPID 10
#define SYS_SLEEP 11

#define NSYSCALLS 12 // not a syscall: last syscall + 1, used for range checks

#endif