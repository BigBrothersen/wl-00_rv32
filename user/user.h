// The header every user C program includes: syscall wrappers (usys.S),
// string/memory helpers (ulib.c) and printf (printf.c).
#ifndef __USER_H__
#define __USER_H__

// syscalls
void exit(int status) __attribute__((noreturn));
int fork(void);
int wait(int *status); // status may be 0
int exec(const char *path, char *argv[]);
int read(int fd, void *buf, int n);
int write(int fd, const void *buf, int n);
int getpid(void);
int sleep(int ticks);
int kill(int pid);

// ulib.c
unsigned int strlen(const char *s);
int strcmp(const char *a, const char *b);
void *memset(void *dst, int c, unsigned int n);
void *memcpy(void *dst, const void *src, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);

// printf.c
void printf(const char *fmt, ...);

#endif