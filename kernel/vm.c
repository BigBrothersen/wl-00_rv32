// #include "kprint.h" // debug
#include "defs.h"
#include "uart.h"
#include "mem.h"
#include "csr.h"
#include "vm.h"
#include "proc.h"
#include "syscall.h"

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
        return 0;
    }
    if ((pa % PAGE_SIZE) != 0) {
        error("mappages: pa not aligned");
        return 0;
    }
    if ((size % PAGE_SIZE) != 0) {
        error("mappages: size not aligned");
        return 0;
    }
    if (size == 0) {
        error("mappages: size 0");
        return 0;
    }

    // Calculate mapping boundaries
    uint32_t end = va + size;
    pte_t *pte;
    int ret = 0;

    // Map each page in the range
    for (uint32_t curr_va = va; curr_va < end; curr_va += PAGE_SIZE, pa += PAGE_SIZE) {
        pte = mappage(pt, curr_va, pa, flags);
        if (!pte) {
            error("mappages: failed to map va to pa");
            ret = -1;
            break;
        }
    }
    return ret;
}

// Map a single page in page table
pte_t *mappage(pagetable_t pt, uint32_t va, uint32_t pa, int flags) {
    if ((va % PAGE_SIZE) != 0)
        error("mappage: va not aligned");
    if ((pa % PAGE_SIZE) != 0)
        error("mappage: pa not aligned");

    uint32_t vpn1 = (va >> 22) & 0x3ff; // Shift 22 bits to right
    uint32_t vpn0 = (va >> 12) & 0x3ff; // Shift 12 bits to the right

    if ((pt[vpn1] & PTE_V) == 0) {
        pte_t pt_addr = (pte_t)kalloc();   // Allocate new page for second level page table
        if (!pt_addr)
            error("kalloc not allocated");
        memset((void *)pt_addr, 0, PAGE_SIZE); // Zero out the new page
        pt[vpn1] = ((pt_addr >> 12) << 10) | PTE_V; // Set the (Page Physical Number) and valid bit
    }

    pte_t *table = (uint32_t*)((pt[vpn1] >> 10) << 12);
    table[vpn0] = ((pa >> 12) << 10) | flags | PTE_V;

    return &table[vpn0];
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
    if (va >= MAX_VA_ADDR)
        error("error finding pa");
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

uint32_t find_pa(pagetable_t pt, uint32_t va) {
    if (va >= MAX_VA_ADDR)
        return 0;

    pte_t *pte;
    uint32_t pa;

    pte = find_pte(pt, va, 0);
    // printf("find_pa debug: va=%p, pte_ptr=%p\n", va, pte);
    if (pte == 0)
        return 0;
    if((*pte & PTE_V) == 0)
        return 0;
    if((*pte & PTE_U) == 0)
        return 0;
    pa = PTE2PA(*pte);
    return pa + (va & 0xFFF); 
}

// Takes in virtual memory and copy data piecewise into the real physical page, page by page.
int copyin(pagetable_t pt, char *dst, uint32_t src_va, uint32_t len)
{
    while (len > 0) {
        uint32_t src_pa = find_pa(pt, src_va); // Find the current physical address the current pa actually resides
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

// unmap inside pt, va is start address up to size
void uvmunmap(pagetable_t pt, uint32_t va, uint32_t size, int free) {
    if (va % PAGE_SIZE != 0) {
        error("va not aligned");
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

void uvmfree(pagetable_t pt, uint32_t size) {
    if (size % PAGE_SIZE != 0) {
        error("size not aligned");
    }
    if (size > 0) {
        uvmunmap(pt, 0x1000, size, 1);
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