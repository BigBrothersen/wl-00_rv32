// #include "kprint.h" // debug
#include "defs.h"
#include "uart.h"
#include "mem.h"
#include "csr.h"
#include "vm.h"
#include "proc.h"
#include "syscall.h"

#include "kprint.h"

/*
    Each process will have its own page table (an array of pointers where each pointer points to a direct page within the physical memory or frame)
    VA	Purpose	PA
    0x00001000	User stack	0x80400000
    0x80000000	Kernel text	0x80000000
    0x10000000	UART MMIO	0x10000000

    SV32
    | 31........22 | 21........12 | 11........0 |
    |   VPN[1]     |   VPN[0]     | page offset |
*/

pagetable_t kptable = NULL;

void init_kptable() {
    kptable = kptable_make();
    // printf("kptable %p\n", kptable);
}

// Allocates memory, map the kernel virtual address to physical address
// and returns the address of the kernel page table
pagetable_t kptable_make() {
    pagetable_t kpt;
    kpt = (pagetable_t)kalloc();
    memset(kpt, 0, PAGE_SIZE);

    // Initialize UART
    mappages(kpt, UART_0, UART_0, PAGE_SIZE, PTE_R | PTE_W);
    
    // Map kernel text (read only)
    uint32_t etext_aligned = PGROUNDUP((uint32_t)&_etext);
    uint32_t text_size = etext_aligned - KERNBASE;
    mappages(kpt, KERNBASE, KERNBASE, (uint32_t)text_size, PTE_R | PTE_X);

    // Map the rest 
    mappages(kpt, etext_aligned, etext_aligned, MEM_END-(uint32_t)etext_aligned, PTE_R | PTE_W);

    return kpt;
}

// Map multiple pages for pt, from va up to pa. va, pa, and size must be page aligned
int mappages(pagetable_t pt, uint32_t va, uint32_t pa, uint32_t size, int flags) {
    if ((va % PAGE_SIZE) != 0){
        error("mappages: va not aligned");
        return -1;
    }
    if ((pa % PAGE_SIZE) != 0) {
        error("mappages: pa not aligned");
        return -1;
    }
    if ((size % PAGE_SIZE) != 0) {
        error("mappages: size not aligned");
        return -1;
    }
    if (size == 0) {
        error("mappages: size 0");
        return -1;
    }

    // Calculate mapping boundaries
    uint32_t end = va + size;
    pte_t *pte;
    int ret = 0;

    // Map each page in the range. Compare with != rather than <, because
    // 'end' wraps to 0 when the range reaches the top of the address space.
    for (uint32_t curr_va = va; curr_va != end; curr_va += PAGE_SIZE, pa += PAGE_SIZE) {
        pte = mappage(pt, curr_va, pa, flags);
        if (!pte) {
            error("mappages: failed to map va to pa");
            ret = -1;
            break;
        }
    }
    return ret;
}

// Map a single page in page table. Returns NULL if a second-level table
// could not be allocated.
pte_t *mappage(pagetable_t pt, uint32_t va, uint32_t pa, int flags) {
    if ((va % PAGE_SIZE) != 0)
        panic("mappage: va not aligned");
    if ((pa % PAGE_SIZE) != 0)
        panic("mappage: pa not aligned");

    pte_t *pte = find_pte(pt, va, 1);
    if (pte == 0)
        return 0;
    if (*pte & PTE_V)
        panic("mappage: remap of an already-mapped va");
    *pte = PA2PTE(pa) | flags | PTE_V;

    return pte;
}

void set_satp(pagetable_t pt) {
    uint32_t root_pa = (uint32_t)pt;
    uint32_t root_ppn = (root_pa >> 12) & 0x003FFFFF;  // 22 bits only
    uint32_t satp_val = (1 << 31) | root_ppn;  // MODE=1 (Sv32)
    w_satp(satp_val);
    __asm__ volatile("sfence.vma zero, zero");
}

// Set satp CSR as the kernel page table during booting
void init_kvmhart() {
    set_satp(kptable);
}

// Allocates a single physical page to store the page table
pagetable_t ptcreate() {
    pagetable_t pt;
    pt = (pagetable_t)kalloc();
    if (pt == NULL)
        return 0;
    memset(pt, 0, PAGE_SIZE);
    return pt;
}

// Debug: translate VA to PA according to page table.
void va2pa(pagetable_t pt, uint32_t va)
{
    uint32_t vpn1 = (va >> 22) & 0x3ff; // shift 22 bits to right
    uint32_t vpn0 = (va >> 12) & 0x3ff;
    pte_t *table = (uint32_t*)((pt[vpn1] >> 10) << 12); // 
    uint32_t mapped_pa = (table[vpn0] >> 10) << 12;
    printf("mapping of va %p to pa %p\n", va, mapped_pa);
}

// Debug: check if paging is enabled
int paging_status() {
    uint32_t x = r_satp();
    return (x >> 31) & 1;
}

// Returns virtual address of the PTE
pte_t *find_pte(pagetable_t pt, uint32_t va, int alloc) 
{
    uint32_t vpn1 = (va >> 22) & 0x3ff; // shift 22 bits to right
    uint32_t vpn0 = (va >> 12) & 0x3ff;
    pte_t *pte1 = &pt[vpn1];
    if (*pte1 & PTE_V) {
        pt = (pagetable_t)PTE2PA(*pte1);
    }
    else {
        if (!alloc)
            return 0;
        pt = (pagetable_t)kalloc();
        if (pt == 0) {
            error("find_pte: NULL address");
            return 0;
        }
        memset(pt, 0, PAGE_SIZE);
        *pte1 = PA2PTE((uint32_t)pt) | PTE_V;
    }
    return &pt[vpn0];
}

// Translate a user va. Returns 0 unless the page is valid, user-accessible,
// and has every permission bit in 'need' (e.g. PTE_W before a kernel write).
static uint32_t find_user_pa(pagetable_t pt, uint32_t va, int need) {
    pte_t *pte = find_pte(pt, va, 0);
    if (pte == 0)
        return 0;
    if ((*pte & (PTE_V | PTE_U | need)) != (PTE_V | PTE_U | need))
        return 0;
    return PTE2PA(*pte) + (va & 0xFFF);
}

// Translate a user va to its pa (0 if unmapped or not user-accessible).
uint32_t find_pa(pagetable_t pt, uint32_t va) {
    return find_user_pa(pt, va, 0);
}

// Takes in virtual memory and copy data piecewise into the real physical page, page by page.
int copyin(pagetable_t pt, char *dst, uint32_t src_va, uint32_t len)
{
    while (len > 0) {
        uint32_t src_pa = find_user_pa(pt, src_va, PTE_R); // Find the current physical address the current pa actually resides
        if (src_pa == 0)
            return -1;
        uint32_t offset = src_pa % PAGE_SIZE;
        uint32_t n = PAGE_SIZE - offset;
        if (n > len)
            n = len;
        memmove(dst, (void *)(uintptr_t)src_pa, n);
        len -= n;
        dst += n;
        src_va += n;
    }
    return 0;
}

// Takes in kernel memory and copies it piecewise into user virtual memory, page by page.
int copyout(pagetable_t pt, uint32_t dst_va, char *src, uint32_t len)
{
    while (len > 0) {
        uint32_t dst_pa = find_user_pa(pt, dst_va, PTE_W); // never let the kernel write into user text
        if (dst_pa == 0)
            return -1;
        uint32_t offset = dst_pa % PAGE_SIZE;
        uint32_t n = PAGE_SIZE - offset;
        if (n > len)
            n = len;
        memmove((void *)(uintptr_t)dst_pa, src, n);
        len -= n;
        src += n;
        dst_va += n;
    }
    return 0;
}

// Copy a NUL-terminated string from user virtual address src_va into kernel
// buffer dst, page by page, copying at most max bytes including the NUL.
// In principle similar to copyin, except it stops at the NUL.
// Returns 0 on success, -1 if a page is not user-readable or no NUL was
// found within max bytes.
int copyinstr(pagetable_t pt, char *dst, uint32_t src_va, uint32_t max) {
    int null = 0;
    while (!null && max > 0) {
        uint32_t src_pa = find_user_pa(pt, src_va, PTE_R);
        if (src_pa == 0)
            return -1;
        uint32_t offset = src_pa % PAGE_SIZE;
        uint32_t n = PAGE_SIZE - offset;
        if (n > max)
            n = max;
        src_va += n; // next pass starts on the next page (the loop below counts n down)
        char *p = (char *)(src_pa);
        while (n > 0) {
            if (*p == '\0') {
                *dst = '\0';
                null = 1;
                break;
            }
            else {
                *dst = *p;
            }
            n--;
            max--;
            p++;
            dst++;
        }
    }
    if (null) {
        return 0;
    }
    else {
        return -1;
    }
}

// unmap inside pt, va is start address up to size
void uvmunmap(pagetable_t pt, uint32_t va, uint32_t size, int free) {
    if (va % PAGE_SIZE != 0 || size % PAGE_SIZE != 0) {
        panic("uvmunmap: not aligned");
    }
    for (uint32_t curr = va; curr < va + size; curr += PAGE_SIZE) {
        pte_t *pte = find_pte(pt, curr, 0);
        if (pte && (*pte & PTE_V)) {
            if (free) {
                uint32_t pa = PTE2PA(*pte);
                kfree((void *)pa);
            }
            *pte = 0;
        }
    }
}

// Free a user address space. 'size' is p->sz: the end va of the user image,
// which occupies [USER_BASE, size).
void uvmfree(pagetable_t pt, uint32_t size) {
    if (size % PAGE_SIZE != 0) {
        panic("uvmfree: size not aligned");
    }
    if (size > USER_BASE) {
        uvmunmap(pt, USER_BASE, size - USER_BASE, 1);
    }
    uvmunmap(pt, USER_STACK_TOP - PAGE_SIZE, PAGE_SIZE, 1);
    freewalk(pt);
}

void freewalk(pagetable_t pt) {
    for (int i = 0; i < 1024; i++) {
        pte_t pte1 = pt[i];
        if (pte1 & PTE_V) {
            pte_t* pt_2 = (pte_t *)PTE2PA(pte1); 
            // for (int j = 0; j < 1024; j++) {
            //     if (pt_2[j] & PTE_V) {
            //         kfree((void *)PTE2PA(pt_2[j]));
            //     }
            // }
            kfree((void *)pt_2); 
        }
    }
    kfree((void *)pt);
}

// Grow a user address space from oldsz to newsz by mapping zeroed pages with
// permissions PTE_R | PTE_U | xperm. Returns newsz on success, 0 on failure
// (after freeing every page this call mapped, so nothing leaks).
// The caller must keep newsz inside the user range: mappage() panics on a remap.
uint32_t uvmalloc(pagetable_t pt, uint32_t oldsz, uint32_t newsz, int xperm) {
    if (oldsz > newsz) return oldsz;

    char *mem;
    uint32_t start = PGROUNDUP(oldsz);

    for (uint32_t curr = start; curr < newsz; curr += PAGE_SIZE) {
        mem = kalloc();
        if (mem == NULL) {
            uvmunmap(pt, start, curr - start, 1); // undo the pages mapped so far
            return 0;
        }
        memset(mem, 0, PAGE_SIZE);
        if (mappages(pt, curr, (uint32_t)mem, PAGE_SIZE, PTE_R | PTE_U | xperm) != 0) {
            kfree(mem);
            uvmunmap(pt, start, curr - start, 1);
            return 0;
        }
    }
    return newsz;
}