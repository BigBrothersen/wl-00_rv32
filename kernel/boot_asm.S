# load kernel into 0x80000000
.section .text
.global _entry
_entry:
    # stack size = 4096B
    csrr a0, mhartid // Get hardware id
    la sp, stack_bottom
    li t0, 4096
    mul t0, a0, t0
    sub sp, sp, t0
    addi tp, a0, 0
    call boot       # jump to boot.c
    j loop          # should never go here

# .section .text
# .global _main
# _main:
#     call main
#     j .

loop:
    j loop
