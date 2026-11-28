// The header every user C program includes: syscall wrappers (usys.S),
// string/memory helpers (ulib.c) and printf (printf.c).
#ifndef __USER_H__
#define __USER_H__

// defs
#define NULL ((void *)0)
#define MAXARGS 16
#define MAXBUF 256
#define INT_MAX 0x7fffffff
#define INT_MIN (-INT_MAX - 1)

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
char *strtok(char *str, const char *delim);
unsigned int strlen(const char *s);
int strcmp(const char *a, const char *b);
void *memset(void *dst, int c, unsigned int n);
void *memcpy(void *dst, const void *src, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);
int usergetpid(void);
int stoi(char *str);
int isdigit(char c);

// printf.c
void printf(const char *fmt, ...);

#endif