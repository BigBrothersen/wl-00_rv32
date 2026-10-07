# load kernel into 0x80000000
#define NCPU 4  // keep in sync with defs.h and the stack reservation in kernel.ld

.section .text.entry
.global _entry
_entry:
    # stack size = 4096B
    csrr a0, mhartid // Get hardware id (First instruction executed)

    # Harts beyond NCPU have no stack or struct cpu reserved for them: park them.
    li t0, NCPU
    bgeu a0, t0, park

    la sp, stack_bottom
    li t0, 4096
    mul t0, a0, t0
    sub sp, sp, t0
    addi tp, a0, 0

    # Hart 0 zeroes .bss. QEMU's ELF loader already does this, but a raw binary
    # loaded by real firmware will not. Other harts only touch .data before
    # main() releases them (see 'started' in main.c).
    bnez a0, 2f
    la t0, _bss_start
    la t1, _bss_end
1:
    bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:
    call boot       # jump to boot.c
    j park          # should never go here

park:
    wfi
    j park
