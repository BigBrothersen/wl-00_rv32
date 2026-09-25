# Second user program, started by init through sys_exec.
# Entered with a0 = argc and a1 = argv; prints a greeting followed by
# each argument on its own line, then exits with status argc.
.global start

start:
    mv s0, a0           # argc
    mv s1, a1           # argv
    li s2, 0            # i

    li a0, 1
    la a1, msg_hello
    li a2, 17
    li a7, 7            # sys_write
    ecall

print_arg:
    bge s2, s0, done
    slli t0, s2, 2
    add t0, s1, t0
    lw a1, 0(t0)        # argv[i]

    mv a2, zero         # strlen(argv[i])
1:
    add t1, a1, a2
    lbu t1, 0(t1)
    beqz t1, 2f
    addi a2, a2, 1
    j 1b
2:
    li a0, 1
    li a7, 7            # sys_write
    ecall

    li a0, 1
    la a1, newline
    li a2, 1
    li a7, 7            # sys_write
    ecall

    addi s2, s2, 1
    j print_arg

done:
    mv a0, s0           # exit status = argc
    li a7, 1            # sys_exit
    ecall

# Pad the image past one page so the strings below live on the second
# page, exercising multi-page loading.
.space 4096

msg_hello:
    .string "Hello from exec: "

newline:
    .string "\n"
