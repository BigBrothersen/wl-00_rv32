#ifndef __ELF_H__
#define __ELF_H__
#include <stdint.h>

#define ELF_MAGIC 0x464C457F  // "\x7FELF" in little endian


// Based on https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.eheader.html
struct elf_hdr {
    uint32_t magic;
    unsigned char e_ident[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};
_Static_assert(sizeof(struct elf_hdr) == 52, "");



// Based on https://refspecs.linuxbase.org/elf/gabi4+/ch5.pheader.html
struct prg_hdr {
    uint32_t type;
    uint32_t offset;
    uint32_t va;
    uint32_t pa;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
};

_Static_assert(sizeof(struct prg_hdr) == 32, "");

#define PT_LOAD 1

#define PF_X 0x1 // execute
#define PF_W 0X2 // write
#define PF_R 0x4 // read

#define ET_EXEC 2
#define EM_RISCV 243
#define ELFCLASS32 1

#endif