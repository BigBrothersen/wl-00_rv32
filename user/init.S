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

    # Replace the child with the hello program. exec only returns if it
    # failed: then the child falls through and prints "Child" instead.
    la a0, path_hello
    la a1, argv_hello   # argv = {"hello", "from", "argv", 0}
    li a7, 4    # sys_exec
    ecall

    li a0, 1
    la a1, msg_child
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

path_hello:
    .string "hello"

arg_from:
    .string "from"
arg_argv:
    .string "argv"

    .align 2            # 4-byte align the array (2^2 bytes)
argv_hello:
    .word path_hello, arg_from, arg_argv, 0