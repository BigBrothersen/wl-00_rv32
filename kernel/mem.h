#ifndef __MEM__
#define __MEM__

/*
    All macros and functions relating to physical memory management.
*/

// Includes
#include <stdint.h>

// Definitions for memory boundary, page size, and function to round up/down pages

// Physical Memory Layout
#define MEM_START 0x80000000ULL
#define KERNBASE 0x80000000ULL      
#define MEM_END 0x88000000ULL       // 128 MB RAM

// Pages
#define PAGE_SIZE 4096              // 4KB page
#define PAGE_SHIFT 12               // number of bits needed to change to next page 
// #define PAGE_SIZE_HEX 0x1000
#define NUM_PAGES ((MEM_END-MEM_START)/PAGE_SIZE)

// Aligment
#define PGROUNDUP(sz)  (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PAGE_SIZE-1))

// Functions
void print_mem(uint32_t addr, int len, int increment);  // debug
void init_bitmap();
void kfree_range(void *start_pa, void *end_pa);
void *kalloc();
void kfree(void *pa);
void *memset(void *ptr, int value, uint32_t num);
void* memmove(void *dst, const void *src, uint32_t n);
int page_idx(void *pa);
uint8_t page_empty(int i);

#endif