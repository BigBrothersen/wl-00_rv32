#include <stdint.h>
#include "kprint.h"
#include "defs.h"
#include "mem.h"
// #include "mutex.h"
// Kernel file for memory management
// Allocates physical memory

// Bitmap of available physical memories in form of pages. 1 is available, 0 is occupied page.
static uint8_t mem_bitmap[NUM_PAGES];
static char *start_page;

// Checks if page at bitmap i is empty
uint8_t page_empty(int i) {
    return mem_bitmap[i];
}

// Debug: to print the memory values according to the increment
void print_mem(uint32_t addr, int len, int increment)
{
    if (!(increment == 1 || increment == 2 || increment == 4)){
        printf("Invalid increment");
        return;
    }
    uint8_t *ptr = (uint8_t *) addr;
    int i;
    for (i = 0; i < len; i += increment) {
        void *curr_addr = (void *)(ptr+i);
        printf("%p: ", curr_addr);
        switch (increment){
            case 1: {
                uint8_t *val = (uint8_t *)curr_addr; // 1 byte
                printf("%p\n", *val);
                break;
            }
            case 2: {
                uint16_t *val = (uint16_t *)curr_addr; // 1 byte
                printf("%p\n", *val);
                break;
            }
            case 4: {
                uint32_t *val = (uint32_t *)curr_addr; // 1 byte
                printf("%p\n", *val);
                break;
            }
        }
    }
}

// Initializes the bitmap, only called during booting phase
void init_bitmap(){
    start_page = (char *)PGROUNDUP((uintptr_t)end);
    int i;
    for (i = 0; i < page_idx(start_page); i++) {
        mem_bitmap[i] = 0;
    }
    kfree_range(start_page, (void *)MEM_END);
}

// Free all pages from start_pa to end_pa
void kfree_range(void *start_pa, void *end_pa){
    if (start_pa >= end_pa) return;
    char *curr = (char *)PGROUNDUP((uintptr_t)start_pa);
    char *end = (char *)PGROUNDDOWN((uintptr_t)end_pa);
    do {
        kfree(curr);
        curr += PAGE_SIZE;
    } while (curr < (char *)end);
}

// Returns page index of a physical address. Must be larger than KERNBASE.
int page_idx(void *pa) {
    char *addr = (char *)PGROUNDDOWN((uintptr_t)pa);
    int idx;
    idx = ((addr - (char *)MEM_START)/PAGE_SIZE);
    if (idx < 0) return -1;
    return idx;
}

// Fill blocks of memory up to num with certain values
void *memset(void *ptr, int value, uint32_t num) {
    char *curr = (char *)ptr;
    int i;
    for (i = 0; i < num; i++, curr += 1) {
        *curr = value;
    }
    return (char *)ptr;
}

// Function to free the physical memory pointed by the page address
void kfree(void *pa){
    int i = page_idx(pa);
    if (i < 0 || i > NUM_PAGES-1) {
        error("failed to free memory");
        return;
    }
    mem_bitmap[i] = 1;
    memset((void *)PGROUNDDOWN((uintptr_t)pa), 0, PAGE_SIZE); // Set all pages to 0
    pa = NULL; // set pointer back to NULL
}

// Allocates a single page of physical memory. Returns a pointer pointing to the allocated page.
// If process fails return null pointer.
void *kalloc(){
    char *addr;
    for (int i = 0; i < NUM_PAGES; i++) {
        if (mem_bitmap[i]) {
            mem_bitmap[i] = 0;
            addr = (char*)(MEM_START + i*PAGE_SIZE);
            // printf("allocated page %d %d with address %p", i, page_idx(addr), addr);
            return addr;
        }
    } 
    return NULL;
}

// TODO: change later so no plagiarism
void* memmove(void *dst, const void *src, uint32_t n)
{
  const char *s;
  char *d;

  if(n == 0)
    return dst;
  
  s = src;
  d = dst;
  if(s < d && s + n > d){
    s += n;
    d += n;
    while(n-- > 0)
      *--d = *--s;
  } else
    while(n-- > 0)
      *d++ = *s++;

  return dst;
}