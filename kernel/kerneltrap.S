.section .text
.global kerneltrap
.align 4
kerneltrap:
    addi sp, sp, -128

    sw ra, 0(sp)
    sw gp, 8(sp)
    sw tp, 12(sp)
    
    # Save Temporaries (Caller-Saved)
    sw t0, 16(sp)
    sw t1, 20(sp)
    sw t2, 24(sp)
    sw t3, 28(sp)
    sw t4, 32(sp)
    sw t5, 36(sp)
    sw t6, 40(sp)
    
    # Save Arguments (Caller-Saved)
    sw a0, 44(sp)
    sw a1, 48(sp)
    sw a2, 52(sp)
    sw a3, 56(sp)
    sw a4, 60(sp)
    sw a5, 64(sp)
    sw a6, 68(sp)
    sw a7, 72(sp)

    # Save Saved Registers (Callee-Saved) - CRITICAL
    sw s0, 76(sp) # Frame Pointer
    sw s1, 80(sp)
    sw s2, 84(sp)
    sw s3, 88(sp)
    sw s4, 92(sp)
    sw s5, 96(sp)
    sw s6, 100(sp)
    sw s7, 104(sp)
    sw s8, 108(sp)
    sw s9, 112(sp)
    sw s10, 116(sp)
    sw s11, 120(sp)

    # 3. Call C Handler
    csrr a0, scause
    csrr a1, sepc
    call s_trap_handle

    # 4. Restore Context
    lw ra, 0(sp)
    lw gp, 8(sp)
    lw tp, 12(sp)
    
    lw t0, 16(sp)
    lw t1, 20(sp)
    lw t2, 24(sp)
    lw t3, 28(sp)
    lw t4, 32(sp)
    lw t5, 36(sp)
    lw t6, 40(sp)
    
    lw a0, 44(sp)
    lw a1, 48(sp)
    lw a2, 52(sp)
    lw a3, 56(sp)
    lw a4, 60(sp)
    lw a5, 64(sp)
    lw a6, 68(sp)
    lw a7, 72(sp)

    lw s0, 76(sp)
    lw s1, 80(sp)
    lw s2, 84(sp)
    lw s3, 88(sp)
    lw s4, 92(sp)
    lw s5, 96(sp)
    lw s6, 100(sp)
    lw s7, 104(sp)
    lw s8, 108(sp)
    lw s9, 112(sp)
    lw s10, 116(sp)
    lw s11, 120(sp)

    addi sp, sp, 128

    sret