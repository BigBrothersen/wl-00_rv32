#ifndef __SYSCALL__
#define __SYSCALL__
#include <stdint.h>

// Functions
void syscall();

// Define
#define SYS_MAXLIMIT 10
// #define SYS_EXIT 1
// #define SYS_OPEN 2
// #define SYS_WAIT 3
// #define SYS_EXEC 4
// #define SYS_FORK 5
// #define SYS_READ 6
// #define SYS_WRITE 7
// #define SYS_BRK 8
// #define SYS_KILL 9
// #define SYS_GETPID 10

typedef enum {
    SYS_EXIT = 1,
    SYS_OPEN,
    SYS_WAIT,
    SYS_EXEC,
    SYS_FORK,
    SYS_READ,
    SYS_WRITE,
    SYS_BRK,
    SYS_KILL,
    SYS_GETPID  // Designate SYS_GETPID as the last sysnum
} sysnums;

#endif