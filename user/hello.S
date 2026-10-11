# Second user program, exec's first target: prints a message, then its
# arguments one per line, then exits.
.global start

start:
    # exec starts us with a0 = argc and a1 = argv. Every ecall overwrites a0
    # with its return value, so keep both in s0/s1, which syscalls leave alone.
    mv s0, a0
    mv s1, a1

    li a0, 1
    la a1, msg_hello
    li a2, 16
    li a7, 7        # sys_write
    ecall

loop:
    beqz s0, exit   # no arguments left
    lw t0, 0(s1)    # t0 = address of this argument
    mv t1, t0
measure:            # t1 walks to the string's terminating 0
    lbu t2, 0(t1)
    beqz t2, print
    addi t1, t1, 1
    j measure
print:
    li a0, 1
    mv a1, t0
    sub a2, t1, t0  # length = end - start
    li a7, 7        # sys_write
    ecall

    li a0, 1
    la a1, newline
    li a2, 1
    li a7, 7        # sys_write
    ecall

    addi s1, s1, 4  # next argv entry
    addi s0, s0, -1
    j loop

exit:
    li a0, 7        # exit status
    li a7, 1        # sys_exit
    ecall

msg_hello:
    .string "Hello from exec\n"

newline:
    .string "\n"
