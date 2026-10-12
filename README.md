# wl-00_RV32

wl-00_RV32 is a small multi-core operating system for 32-bit RISC-V, written
from scratch and heavily inspired by [xv6-riscv](https://github.com/mit-pdos/xv6-riscv).
It runs on QEMU's `virt` machine.

## Features

Implemented so far:

- **Bare-metal boot.** The kernel starts in machine mode with no firmware (no
  OpenSBI), configures the CSRs (trap delegation, PMP, the Sstc timer), then
  switches to the supervisor-mode kernel, which runs programs in user mode.
- **Bitmap physical memory allocator.** One byte per 4 KB page records whether
  the page is free. xv6 uses a linked list threaded through the free pages instead.
- **Kernel memory primitives:** page allocation (`kalloc`/`kfree`) and the usual
  helpers (`memset`, `memmove`, `memcpy`).
- **Virtual memory** with Sv32 two-level page tables: one page table for the
  kernel and one per process.
- **Traps and interrupts:**
  - safe switches between user and kernel mode, saving and restoring each
    process's registers in its trapframe;
  - system calls through `ecall`;
  - a 100 Hz timer interrupt (Sstc) that drives preemption and `sleep`;
  - a user process that faults is killed with a diagnostic instead of crashing
    the kernel.
- **Spinlocks** for synchronization between harts.
- **Preemptive scheduler** running on up to 4 harts (CPU cores).
- **System calls:** `fork`, `exec`, `wait`, `exit` and `sleep`.
- **User programs in C,** loaded from ELF images and started with `argc`/`argv`.

## Running it

**Prerequisites**

- A RISC-V GCC toolchain: `riscv32-unknown-elf-gcc`, or a multilib
  `riscv64-unknown-elf-gcc` (build with `make TOOLPREFIX=riscv64-unknown-elf-`).
- `qemu-system-riscv32` with support for the Sstc extension. Tested with QEMU 11.1.2.
- `xxd`, which the build uses to embed the user programs in the kernel image.

**Commands**

| Command | What it does |
|---|---|
| `make qemu` | Build and boot with 4 harts |
| `make qemu CPUS=1` | Boot with a single hart |
| `make qemu-gdb` | Boot paused, waiting for a debugger on port 1234 |
| `make clean` | Remove all build outputs |

Exit QEMU with Ctrl-A, then X. To debug, run `make qemu-gdb`, then in a second
terminal: `gdb -ex "target remote :1234" kernel.elf`.

## How it works

### Boot flow

```
_entry        boot_asm.S   every hart: its own boot stack, tp = hart id;
                           harts beyond 4 are parked; hart 0 zeroes .bss
   |
   v
boot()        boot.c       M-mode: delegate traps to S-mode, open PMP,
                           enable Sstc, set the first timer deadline, mret
   |
   v
main()        main.c       S-mode: hart 0 initializes the kernel and creates
                           init; the other harts wait, then turn on paging
   |
   v
scheduler()   proc.c       every hart runs READY processes, forever
```

### Physical memory map

```
0x88000000  -- end of RAM (128 MB)
               free pages, handed out by the bitmap allocator
end         --
               boot stacks: 4 x 4 KB, one per hart
               4 KB gap
               .bss
               .rodata, .data
               trampoline page: trap entry/exit code
0x80000000  -- kernel text (QEMU loads the kernel here)

0x10000000  -- UART
```

### User address space

Every process has its own page table:

```
0xFFFFF000  TRAMPOLINE slot  \  reserved, not mapped
0xFFFFE000  TRAPFRAME slot   /
0xFFFFD000  -- USER_STACK_TOP
               user stack: one page (R+W)
0xFFFFC000  --
               (unmapped)
0x88000000  --
               kernel and RAM: identity-mapped, supervisor-only
0x80000000  --
               (unmapped)
0x10000000  -- UART: identity-mapped, supervisor-only; top of the user range
               program data and .bss (R+W)
               program text (R+X)
0x00001000  -- USER_BASE: programs are loaded here
               page 0: unmapped, so null-pointer accesses fault
0x00000000  --
```

The kernel is mapped, without user access, into every page table, so the trap
entry and exit code runs at the same address whichever page table is active.

### Trap path

```
user program: ecall, interrupt or fault
   |
   v
usertrap.S       save the registers in the process's trapframe (found
                 through sscratch), switch satp to the kernel page table
   |
   v
u_trap_handle()  ecall      -> syscall()
                 interrupt  -> devintr()   (timer tick: yield the CPU)
                 fault      -> kexit(-1)   (the process is killed)
   |
   v
utrapret()       switch satp back to the process's page table
   |
   v
usertrapret.S    restore the registers, sret back to user mode
```

### Processes

- States: `UNUSED -> NEW -> READY <-> RUNNING`, plus `WAITING` (asleep) and
  `ZOMBIE` (exited but not yet reaped).
- `fork` copies the parent's memory page by page, eagerly (no copy-on-write).
- `exit` hands the process's children to `init` (pid 1). `wait` reaps an exited
  child and returns its exit status.
- `sleep` and `wakeup` work on channels. Locks are always taken in the order
  `wait_lock`, then `p->lock`.
- Scheduling is round-robin over the process table. The timer preempts the
  running process on every tick, and an idle hart waits with `wfi`.

### exec

`exec` builds the complete new address space first, then switches the process
over to it, and only then frees the old one. A failed `exec` therefore leaves
the caller untouched, and it gets -1 back. The arguments are copied into a
kernel page first, then written onto the new user stack, keeping the stack
pointer 16-byte aligned as the RISC-V calling convention requires.

### System calls

| No. | Name | Status |
|---|---|---|
| 1 | `exit` | Implemented |
| 2 | `open` | Stub, returns -1 |
| 3 | `wait` | Implemented |
| 4 | `exec` | Implemented |
| 5 | `fork` | Implemented |
| 6 | `read` | Stub, returns -1 |
| 7 | `write` | Implemented (fd 1 only) |
| 8 | `brk` | Stub, returns -1 (next to be implemented) |
| 9 | `kill` | Stub, returns -1 |
| 10 | `getpid` | Implemented |
| 11 | `sleep` | Implemented (in ticks, 100 per second) |

### Writing a user program

There is no filesystem yet, so user programs are built into the kernel image.
For example, `user/echo.c`:

```c
// echo: print the arguments separated by spaces, then a newline.
// argv[0] is the program's own name, so printing starts at argv[1].
#include "user.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (i < argc - 1) printf("%s ", argv[i]);
        else printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}
```

To add a program called `foo`:

1. Write `user/foo.c` with a `main(int argc, char *argv[])`.
2. In the Makefile, add `$U/foo.elf` to the prerequisites of the
   `$(INITCODE_HEADER)` rule, and add the line `xxd -i $U/foo.elf >> $@` under it.
3. In `kernel/exec.c`, add `{ "foo", user_foo_elf, sizeof(user_foo_elf) },` to
   the `programs[]` table.

It can then be started with `exec("foo", argv)`, for example through `run()` in
`user/init.c`.

## References

- [xv6-riscv](https://github.com/mit-pdos/xv6-riscv), MIT's teaching operating system
- [The RISC-V ISA manuals](https://github.com/riscv/riscv-isa-manual), including the
  privileged specification (Sv32 paging, the Sstc extension)
- [QEMU's RISC-V `virt` machine](https://www.qemu.org/docs/master/system/riscv/virt.html)
- [Linux Source Code](https://elixir.bootlin.com/)
- [CUHKSZ]CSC3150 (Operating Systems) & CSC3050 (Computer Architecture) Slides & Homeworks
