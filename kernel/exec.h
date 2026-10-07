#ifndef __EXEC__
#define __EXEC__
#include <stdint.h>
#include "vm.h"

int load_elf(pagetable_t pt, const uint8_t *img, uint32_t len, uint32_t *entry, uint32_t *sz);
int load_segment(pagetable_t pt, uint32_t va, const uint8_t *img, uint32_t offset, uint32_t sz);

#endif