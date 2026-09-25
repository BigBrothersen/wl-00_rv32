#include <stdint.h>
#include "kprint.h"
#include "defs.h"
#include "mem.h"
#include "spinlock.h"
// Kernel file for memory management
// Allocates physical memory

// Bitmap of available physical memories in form of pages. 1 is available, 0 is occupied page.
static uint8_t mem_bitmap[NUM_PAGES];
static char *start_page;
static struct spinlock mem_lock; // guards mem_bitmap; every hart allocates

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
    init_lock(&mem_lock, "mem");
    start_page = (char *)PGROUNDUP((uintptr_t)end);
    int i;
    for (i = 0; i < page_idx(start_page); i++) {
        mem_bitmap[i] = 0;
    }
    kfree_range(start_page, (void *)MEM_END);
}

// Free all pages from start_pa to end_pa
void kfree_range(void *start_pa, void *end_pa){
    char *curr = (char *)PGROUNDUP((uintptr_t)start_pa);
    char *end = (char *)PGROUNDDOWN((uintptr_t)end_pa);
    while (curr < end) {
        kfree(curr);
        curr += PAGE_SIZE;
    }
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
    if ((uintptr_t)pa % PAGE_SIZE != 0)
        panic("kfree: address not page aligned");
    if ((char *)pa < start_page || (uintptr_t)pa >= MEM_END)
        panic("kfree: address outside the allocatable range");
    int i = page_idx(pa);
    memset(pa, 0, PAGE_SIZE); // Set all pages to 0 (before publishing it as free)
    lock(&mem_lock);
    if (mem_bitmap[i]) {
        unlock(&mem_lock);
        panic("kfree: double free");
    }
    mem_bitmap[i] = 1;
    unlock(&mem_lock);
}

// Allocates a single page of physical memory. Returns a pointer pointing to the allocated page.
// If process fails return null pointer.
void *kalloc(){
    char *addr;
    lock(&mem_lock);
    for (int i = 0; i < NUM_PAGES; i++) {
        if (mem_bitmap[i]) {
            mem_bitmap[i] = 0;
            unlock(&mem_lock);
            addr = (char*)(uintptr_t)(MEM_START + i*PAGE_SIZE);
            // printf("allocated page %d %d with address %p", i, page_idx(addr), addr);
            return addr;
        }
    }
    unlock(&mem_lock);
    return NULL;
}

// The compiler may emit calls to memcpy for struct assignment even in
// freestanding mode, so it must exist.
void *memcpy(void *dst, const void *src, uint32_t n)
{
    return memmove(dst, src, n);
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