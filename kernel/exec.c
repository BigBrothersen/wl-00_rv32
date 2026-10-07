#include "exec.h"
#include <stdint.h>
#include "kprint.h"
#include "mem.h"
#include "vm.h"
#include "spinlock.h"
#include "elf.h"
#include "uart.h"

// Load the ELF executable img (len bytes long) into the fresh user page table pt.
// On success sets *entry (the program's first pc) and *sz (page-aligned end of
// the loaded image) and returns 0. Returns -1 if img is not a valid RV32
// executable or memory runs out; *sz then still says how far loading got, so
// the caller can free everything with uvmfree(pt, *sz).
int load_elf(pagetable_t pt, const uint8_t *img, uint32_t len, uint32_t *entry, uint32_t *sz) {
    struct elf_hdr elf;
    struct prg_hdr prg;
    uint32_t size = USER_BASE; // page 0 stays unmapped
    *sz = size;

    // Check the ELF header
    if (len < sizeof(struct elf_hdr)) return -1;
    memmove(&elf, img, sizeof(struct elf_hdr));
    if (elf.magic != ELF_MAGIC) return -1;
    if (elf.e_ident[0] != ELFCLASS32) return -1;
    if (elf.type != ET_EXEC) return -1;
    if (elf.machine != EM_RISCV) return -1;
    if (elf.phentsize != sizeof(struct prg_hdr)) return -1;

    // The program headers are a table of phnum entries, sizeof(struct prg_hdr)
    // bytes each, starting at byte phoff of the file. Each pass reads entry i.
    for (int i = 0; i < elf.phnum; i++) {
        uint32_t off = elf.phoff + i * sizeof(struct prg_hdr);
        if (off < elf.phoff || off + sizeof(struct prg_hdr) > len) return -1; // overflow or past end of file
        memmove(&prg, img + off, sizeof(struct prg_hdr)); // copy out: img may not be 4-byte aligned

        if (prg.type != PT_LOAD) continue;
        if (prg.memsz == 0) continue; // empty segment (e.g. no data at all): nothing to load
        if (prg.memsz < prg.filesz) return -1;
        if (prg.offset + prg.filesz < prg.offset || prg.offset + prg.filesz > len) return -1;
        if (prg.va % PAGE_SIZE != 0) return -1;
        if (prg.va < size) return -1; // overlaps an earlier segment: mappage() would panic on the remap
        // Must end inside user space: above it, every page table maps the UART and the kernel
        if (prg.va + prg.memsz < prg.va || prg.va + prg.memsz > UART_0) return -1;

        int perm = 0; // ELF flag bits differ from PTE bits; uvmalloc adds PTE_R | PTE_U
        if (prg.flags & PF_X) perm |= PTE_X;
        if (prg.flags & PF_W) perm |= PTE_W;
        uint32_t newsz = uvmalloc(pt, size, prg.va + prg.memsz, perm);
        if (newsz == 0) return -1;
        size = newsz;
        *sz = PGROUNDUP(size);

        // Copy the file bytes; the rest up to memsz (.bss) is already zero
        if (load_segment(pt, prg.va, img, prg.offset, prg.filesz) < 0) return -1;
    }

    *entry = elf.entry;
    *sz = PGROUNDUP(size);
    return 0;
}


// Load an ELF program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
// pt already mapped
// va ph.vaddr
// img (LATER FILE READ)
// offset ph.offset
// sz ph.filesz
int load_segment(pagetable_t pt, uint32_t va, const uint8_t *img, uint32_t offset, uint32_t sz) {
    uint32_t pa;
    img += offset;
    while (sz > 0) {
        pa = find_pa(pt, va);
        if (pa == 0) {
            return -1;
        }
        uint32_t pgoff = pa % PAGE_SIZE;
        uint32_t n = PAGE_SIZE - pgoff;
        if (n > sz)
            n = sz;
        memmove((void *)(uintptr_t)pa, img, n);
        sz -= n;
        img += n;
        va += n;
    }
    return 0;
}