// Traps or interrupt happening in u-mode will be sent here
# #include "usertrap.h"
.section .trampoline_text
.global usertrapret
.align 4
usertrapret:   
    lw ra, 24(a0)   # x1
    lw sp, 28(a0)   # x2
    lw gp, 32(a0)   # x3
    lw tp, 36(a0)   # x4
    lw t0, 40(a0)   # x5
    lw t1, 44(a0)   # x6
    lw t2, 48(a0)   # x7
    lw s0, 52(a0)   # x8 / fp
    lw s1, 56(a0)   # x9
    lw a1, 64(a0)   # x11
    lw a2, 68(a0)   # x12
    lw a3, 72(a0)   # x13
    lw a4, 76(a0)   # x14
    lw a5, 80(a0)   # x15
    lw a6, 84(a0)   # x16
    lw a7, 88(a0)   # x17
    lw s2, 92(a0)   # x18
    lw s3, 96(a0)   # x19
    lw s4, 100(a0)  # x20
    lw s5, 104(a0)  # x21
    lw s6, 108(a0)  # x22
    lw s7, 112(a0)  # x23
    lw s8, 116(a0)  # x24
    lw s9, 120(a0)  # x25
    lw s10, 124(a0) # x26
    lw s11, 128(a0) # x27
    lw t3, 132(a0)  # x28
    lw t4, 136(a0)  # x29
    lw t5, 140(a0)  # x30
    lw t6, 144(a0)  # x31

    csrw sscratch, a0

    # load a0 from memory (x10) at offset 60
    lw a0, 60(a0)

    sret