#ifndef __EXEC__
#define __EXEC__
#include <stdint.h>
#include "vm.h"
// #include "initcode.h"

int load_elf(pagetable_t pt, const uint8_t *img, uint32_t len, uint32_t *entry, uint32_t *sz);
int load_segment(pagetable_t pt, uint32_t va, const uint8_t *img, uint32_t offset, uint32_t sz);

// img is a pointer to a pointer: the function hands back where the bytes are
int find_program(const char *name, const uint8_t **img, uint32_t *len);

#define MAXPATH 32 // longest program name, including the NUL
#define MAXARG 16  // most arguments exec accepts
int kexec(const char *path, char **argv);

#endif