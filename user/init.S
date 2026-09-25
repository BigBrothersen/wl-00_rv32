# Initialize user program
.global start

start:
    # sys_write
    # li a0, 1
    # la a1, msg
    # li a2, 16
    # li a7, 7
    # ecall
    # j start

    # sys_fork
    # li a0, 0
    # li a7, 5
    # ecall

    # # sys_exit
    # li a0, 0
    # li a7, 1
    # ecall

    li a7, 5
    ecall

    beqz a0, child_code

parent_code:
    li a0, 1
    la a1, msg_parent
    li a2, 16
    li a7, 7
    ecall

    li a0, 0    # addr = NULL, don't care about the child's status
    li a7, 3    # sys_wait
    ecall

    li a0, 1
    la a1, msg_reaped
    li a2, 16
    li a7, 7
    ecall

    li a0, 0    # exit status
    li a7, 1
    ecall

child_code:
    # Exercise the stack for real (previously fork() never mapped it for
    # the child) instead of just carrying the pointer value around.
    addi sp, sp, -16
    li t0, 0x1234
    sw t0, 0(sp)
    lw t1, 0(sp)
    addi sp, sp, 16

    li a0, 1
    la a1, msg_child
    li a2, 16
    li a7, 7
    ecall

    # exec of an unknown program must fail and return -1
    la a0, path_bogus
    li a1, 0
    li a7, 4    # sys_exec
    ecall
    li t0, -1
    bne a0, t0, exec_failed

    li a0, 1
    la a1, msg_bogus
    li a2, 32
    li a7, 7
    ecall

    # replace this process with "hello", passing two arguments
    la a0, path_hello
    la a1, hello_argv
    li a7, 4    # sys_exec
    ecall

exec_failed:
    # only reached if exec did not behave
    li a0, 1
    la a1, msg_execfail
    li a2, 16
    li a7, 7
    ecall

    li a0, 7    # exit status
    li a7, 1
    ecall


msg:
    .string "FORKING\n"
    # .set msg_len, . - msg

msg_parent:
    .string "Parent\n"

msg_child:
    .string "Child\n"

msg_reaped:
    .string "Reaped\n"

msg_bogus:
    .string "exec of missing program failed\n"

msg_execfail:
    .string "exec failed!\n"

path_bogus:
    .string "nope"

path_hello:
    .string "hello"

arg_world:
    .string "world"

    .align 2
hello_argv:
    .word path_hello
    .word arg_world
    .word 0
