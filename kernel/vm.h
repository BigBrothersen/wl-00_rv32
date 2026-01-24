#ifndef __VM__
#define __VM__

// Includes
#include <stdint.h>

// Page table entries definition and translation functions
typedef uint32_t pte_t;
typedef pte_t* pagetable_t;
#define PTE_V (1ULL << 0) // Valid
#define PTE_R (1ULL << 1) // Readable
#define PTE_W (1ULL << 2) // Writable
#define PTE_X (1ULL << 3) // Executable
#define PTE_U (1ULL << 4) // user can access
#define PTE2PA(pte) ((((uint32_t)(pte)) >> 10) << 12)
#define PA2PTE(pa)  ((((uint32_t)(pa)) >> 12) << 10)
#define PA2VA(pa) ((void*)((uint32_t)(pa) + KERNBASE))

// Virtual memory definitions
#define VIRTUAL_ADDRESS_BITS 32
#define MAX_VA_SIZE_BYTES    (1UL << VIRTUAL_ADDRESS_BITS) // 4 GB
#define MAX_VA_ADDR          (MAX_VA_SIZE_BYTES - 1)       // 0xFFFFFFFF
#define TRAMPOLINE           (MAX_VA_ADDR - PAGE_SIZE + 1)   // store trampoline at the upper virtual address (1 page)
#define TRAPFRAME            (TRAMPOLINE - PAGE_SIZE)  // store trapframe just under the trampoline (1 page)
#define USER_STACK_TOP       (TRAPFRAME - PAGE_SIZE)    // user stack starts in a page just under trapframe (grows downwards)

#define MAKE_SATP(pa)   ((1u << 31) | (((pa) >> 12) & 0x003FFFFF) )

// Global kernel pagetable. Will never change once allocated
extern pagetable_t kptable;

// Functions
void init_kptable();
pagetable_t kptable_make();
pte_t *mappage(pagetable_t pt, uint32_t va, uint32_t pa, int flags);
int mappages(pagetable_t pt, uint32_t va, uint32_t pa, uint32_t size, int flags);
void set_satp(pagetable_t pt);
void init_kvmhart();
pagetable_t ptcreate();
void va2pa(pagetable_t pt, uint32_t va);
int paging_status();
pte_t *find_pte(pagetable_t pt, uint32_t va, int alloc);
uint32_t find_pa(pagetable_t pt, uint32_t va);
int copyin(pagetable_t pt, char *dst, uint32_t src_va, uint32_t len);
#endif