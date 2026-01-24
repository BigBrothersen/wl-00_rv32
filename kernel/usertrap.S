// Traps or interrupt happening in u-mode will be sent here
# #include "usertrap.h"
.section .trampoline_text
.global usertrap
.align 4
usertrap:   
    csrrw a0, sscratch, a0  # Swap sscratch (trapframe ptr) and a0

    sw ra, 24(a0)   # x1
    sw sp, 28(a0)   # x2
    sw gp, 32(a0)   # x3
    sw tp, 36(a0)   # x4
    sw t0, 40(a0)   # x5
    sw t1, 44(a0)   # x6
    sw t2, 48(a0)   # x7
    sw s0, 52(a0)   # x8 / fp
    sw s1, 56(a0)   # x9
    sw a1, 64(a0)   # x11
    sw a2, 68(a0)   # x12
    sw a3, 72(a0)   # x13
    sw a4, 76(a0)   # x14
    sw a5, 80(a0)   # x15
    sw a6, 84(a0)   # x16
    sw a7, 88(a0)   # x17
    sw s2, 92(a0)   # x18
    sw s3, 96(a0)   # x19
    sw s4, 100(a0)  # x20
    sw s5, 104(a0)  # x21
    sw s6, 108(a0)  # x22
    sw s7, 112(a0)  # x23
    sw s8, 116(a0)  # x24
    sw s9, 120(a0)  # x25
    sw s10, 124(a0) # x26
    sw s11, 128(a0) # x27
    sw t3, 132(a0)  # x28
    sw t4, 136(a0)  # x29
    sw t5, 140(a0)  # x30
    sw t6, 144(a0)  # x31

    # save a0 in memory (x10) at offset 60
    csrr t0, sscratch
    sw t0, 60(a0)

    csrr t1, sepc
    sw t1, 16(a0)

    lw sp, 0(a0)    # load kernel stack pointer
    lw t0, 4(a0)    # load kerneltrap
    lw t1, 8(a0)    # load k_satp
    lw t2, 12(a0)   # load u_trap_handle

    # flush TLB and load kernel page table
    sfence.vma zero, zero
    csrw satp, t1
    sfence.vma zero, zero

    csrw stvec, t0  # delegate all traps to kerneltrap, since its in s-mode now

    csrr a0, scause
    csrr a1, sepc
    jr t2