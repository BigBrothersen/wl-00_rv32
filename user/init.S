# Initialize user program
.global start

start:
    li a0, 1
    la a1, msg
    li a2, 16
    li a7, 7
    ecall
    j start

    # li a0, 0
    # li a7, 1
    # ecall

loop:
    j loop


msg:
    .string "grrrrrrrr test!\n"
    # .set msg_len, . - msg