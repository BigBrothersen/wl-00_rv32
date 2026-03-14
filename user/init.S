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

    li a7, 1
    ecall

child_code:
    li a0, 1
    la a1, msg_child
    li a2, 16
    li a7, 7
    ecall

    j loop

loop:
    j loop


msg:
    .string "FORKING\n"
    # .set msg_len, . - msg

msg_parent:
    .string "Parent\n"

msg_child:
    .string "Child\n"