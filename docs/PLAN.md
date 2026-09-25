# wl-00_rv32 — Plan of Work

A concrete roadmap for turning the current kernel into a fully working, self-hosting
32-bit RISC-V operating system, plus an optional path to run it on physical hardware.

Status baseline was taken on 2026-09-25 by reading every file in the tree, building it
with the GCC RISC-V toolchain, and booting it in `qemu-system-riscv32` with 1, 4 and 8 CPUs.

---

## 0. Where the project stands today

### 0.1 What exists and works

| Area | Files | Status |
|---|---|---|
| Boot in M-mode, drop to S-mode, per-hart stacks | `kernel/boot_asm.S`, `kernel/boot.c`, `kernel/kernel.ld` | Works (`-bios none`, all harts reach `main`) |
| CSR / GPR accessors | `kernel/csr.[ch]`, `kernel/gpr.[ch]` | Works, verbose but fine |
| Physical page allocator (bitmap) | `kernel/mem.[ch]` | Works, single-CPU only (no lock) |
| Sv32 paging, kernel page table, `mappages`, `copyin`/`copyout`, `uvmunmap`/`uvmfree` | `kernel/vm.[ch]` | Works |
| Spinlocks with `push_off`/`pop_off` | `kernel/spinlock.[ch]` | Works in shape; see bugs below |
| Kernel `printf` over UART | `kernel/kprint.[ch]`, `kernel/uart.[ch]` | Works on QEMU only (see bugs) |
| Trap plumbing: S-mode trap vector, user trampoline, trapframe | `kernel/kerneltrap.S`, `kernel/usertrap.S`, `kernel/usertrapret.S`, `kernel/trap.c` | Works for `ecall`; exceptions panic; no interrupts |
| Processes, context switch, round-robin scheduler, `fork`, `wait`, `exit` | `kernel/proc.[ch]`, `kernel/swtch.S`, `kernel/syscall.[ch]` | Works: fork → write → wait → exit verified in QEMU |
| Syscalls implemented | `syscall.c` | `exit`, `wait`, `fork`, `write(fd=1)`, `getpid` |
| Syscalls stubbed | `syscall.c` | `open`, `exec`, `read`, `brk`, `kill` return 0 |
| First user program (flat binary embedded via `xxd -i`) | `user/init.S`, `Makefile` | Works |

Verified output (`-smp 1`):

```
wl-00 kernel is booting
Main cpu 0 initialized!
Found new process with pid 1
Parent
Process exited looking for another process to run
Found new process with pid 2
Child
Found new process with pid 1
Reaped
```

Roughly 2,700 lines total. The design closely follows xv6-riscv, which is a good thing:
the remaining pieces have well-understood shapes.

### 0.2 Bugs found (fix these first; they block everything after)

Ranked by how much they will hurt as the system grows.

1. **Hart id is lost on every user→kernel trap (intermittent panic on SMP).**
   `usertrap.S` saves the user's `tp` into the trapframe but never reloads the kernel
   hart id into `tp` before jumping to `u_trap_handle`. `this_cpu()` (`proc.c:15`) reads
   `tp`, so a process picked up by hart 3 is handled as if it were on hart 0.
   Reproduced: intermittently (1 of 6 and 1 of 12 four-CPU boots in testing), whenever
   a hart other than 0 picks the process up, the run ends in
   `KERNEL PANIC! scause: 0xf stval: 0x88000000`. Fix: add `kernel_hartid` to the
   trapframe, store it in `utrapret()`, and `lw tp, off(a0)` in `usertrap.S`
   (this is exactly what xv6's trampoline does).
2. **Interrupts can never be enabled.** `rv.c:10` does `sstatus & SSTATUS_SIE`; it must be
   `sstatus | SSTATUS_SIE`. Every `interrupt_on()` call is a no-op today, which also masks
   bug 1 in most runs.
3. **No machine-mode trap vector, no timer.** `boot.c` never writes `mtvec`, `mie` or the
   CLINT. There is no preemption: a user program that loops forever owns the CPU.
4. **`1UL << 32` in `vm.h:22`.** On rv32 `unsigned long` is 32 bits, so this is undefined
   behaviour. GCC 13 happens to produce the intended `USER_STACK_TOP = 0xFFFFD000`;
   clang 18 treats it as undefined and does not even pass the argument to `uvmfirst`
   (verified in the disassembly of both builds). Replace with literal `0xFFFFF000` etc.
5. **UART driver is for the wrong chip and never initialised.** `uart.h` uses SiFive UART
   offsets (`TXFIFO`, `DIV`) but QEMU `virt` (and most boards) expose an NS16550A.
   `putchar` writes THR without checking LSR; `uart_init` is never called. Works on QEMU
   only because QEMU never drops bytes. No receive path at all.
6. **`NCPU` / stack sizing does not match the documented QEMU command.** The Makefile
   comment says `-smp 8`, but `NCPU` is 4 and the linker script reserves 4 stacks. With
   8 harts, `sp` for harts 4–7 lands in the guard page and `.bss`, and `cpus[tp]` indexes
   out of bounds. Either enforce `-smp $(NCPU)` in `make run` or size both from one macro.
7. **No locking on shared kernel state.** `kalloc`/`kfree` (`mem.c`), `procalloc`,
   `curr_pid` and `wait()`'s scan of `parent` all run without locks. Safe today only
   because bug 2 keeps everything effectively single-threaded.
8. **`wait()` busy-polls with `yield()`.** Fine for now; becomes a CPU burner once timers
   exist. Needs `sleep`/`wakeup`.
9. **`sys_exit` frees the address space before taking `p->lock`,** and the exiting process
   never wakes its parent (follows from 8).
10. **Smaller defects:** `kprint.c:15` `strlen` never advances (infinite loop);
    `gpr.c:4` `r_t0` reads `a0` and returns nothing; `init_proctable` assigns kstacks
    that walk down through `.bss` (they are overwritten by `procalloc`, so harmless
    but misleading); `__attribute__((aligned(8)))` in `proc.h:34` is placed where the
    compiler ignores it; `uvmfirst` does not check `kalloc` failures; `mappage` prints an
    error on `kalloc` failure then dereferences NULL; `kfree` clears every page on free
    (slow, and it assumes the page is not still mapped anywhere).
11. **Build portability.** `fork()`'s struct copy compiles to a `memcpy` call under clang
    and there is no `memcpy` in the kernel. Add one next to `memmove`. The Makefile
    hard-codes `riscv32-unknown-elf-gcc`; the Ubuntu package is `riscv64-unknown-elf-gcc`
    (multilib, builds rv32 fine). Make `CROSS` overridable.

### 0.3 Missing pieces for a "core OS"

- Timer interrupts and preemptive scheduling
- Console input (UART RX, PLIC), `read` on stdin
- `sleep`/`wakeup`, `kill`, `sbrk`
- `exec` and a program loader (flat binary → ELF)
- A user-space runtime: `_start`, syscall stubs, tiny libc, `printf`, `malloc`
- A shell and a few utilities
- Block device (virtio-blk), buffer cache, an on-disk filesystem, file descriptors, pipes
- A test suite that runs in QEMU from `make test`, and CI
- README, `.gitignore`, `make run` / `make gdb`

---

## 1. Definition of done

"Fully working and functional OS" for this project means all of the following hold in
QEMU `virt` with `-smp 4`:

1. The kernel boots, enables paging, starts timer interrupts, and runs `init`.
2. `init` starts a shell on the UART console. A user can type commands.
3. The shell can run programs stored on a disk image: `ls`, `cat`, `echo`, `mkdir`,
   `rm`, `wc`, and pipes (`ls | wc`).
4. Programs can `fork`, `exec`, `wait`, `exit`, `kill`, `sbrk`, `open`/`read`/`write`/
   `close`, `pipe`, `dup`, `chdir`, `mkdir`, `unlink`.
5. A CPU-bound loop in user space does not freeze the shell (preemption works).
6. A page fault or illegal instruction in a user program kills that process, not the
   kernel.
7. `make test` runs an automated suite (`usertests`-style) and passes on every commit in
   CI.

Everything beyond that is an extension (section 4).

---

## 2. Milestones

Each milestone has: tasks, files touched, and an acceptance test you can run. Order is
deliberate: each one is a prerequisite for the next. Time estimates assume part-time
work by one person who already knows the codebase.

### M0 — Foundation and bug fixes (≈1 week)

Goal: a reproducible build, a `make run`, and none of the bugs in §0.2 that will
silently corrupt later work.

Tasks:
- [ ] `Makefile`: `CROSS ?= riscv64-unknown-elf-`, `CC=$(CROSS)gcc`; `make run`,
      `make gdb` (`-s -S`), `make test`; `QEMU_SMP ?= $(NCPU)`; replace `xxd -i` with a
      10-line Python script in `tools/bin2c.py` (fewer host dependencies).
- [ ] Add `.gitignore` (`*.o`, `*.elf`, `*.bin`, `kernel/initcode.h`).
- [ ] Add `README.md` with build/run instructions and a memory map.
- [ ] Fix §0.2 items 2, 4, 6, 10, 11 (all mechanical).
- [ ] Fix item 1: add `kernel_hartid` to `struct trapframe_t`, set it in `utrapret()`,
      reload `tp` in `usertrap.S`. Then boot with `-smp 4` fifty times in a loop; zero
      panics.
- [ ] Add `panic(const char *)` that prints hart id, disables interrupts and spins.
      Replace the `error(...)` calls that currently continue past fatal conditions
      (`mappage` on NULL, `sched` checks, `pop_off`).
- [ ] Add `tools/run_test.sh`: boots QEMU with serial to a file, waits for an expected
      string or a timeout, exits 0/1. Use it for `make test`.
- [ ] GitHub Actions workflow: `apt-get install gcc-riscv64-unknown-elf qemu-system-misc`,
      `make`, `make test`.

Acceptance: `make test` passes locally and in CI on `-smp 1` and `-smp 4`.

### M1 — Interrupts, timer, preemption, console input (≈2 weeks)

Goal: a process can be interrupted; the kernel can read keystrokes.

Tasks:
- [ ] **M-mode timer.** In `boot.c`: for each hart set `mscratch` to a small per-hart
      scratch area, write `mtvec = timervec`, program CLINT `mtimecmp[hart] =
      mtime + INTERVAL`, set `MIE_MTIE` and `MSTATUS_MIE`. New `kernel/timervec.S`
      that re-arms `mtimecmp` and raises a supervisor software interrupt (`sip.SSIP`).
      CLINT on QEMU virt: `0x2000000`, `mtimecmp` at `+0x4000 + 8*hart`, `mtime` at
      `+0xBFF8`; 10 MHz. Put these in `kernel/platform.h` (see §3).
- [ ] **S-mode interrupt dispatch.** In `s_trap_handle` and a new `u_trap_handle`
      interrupt branch: decode `scause` for software (1), timer (5), external (9).
      On timer tick: clear `SSIP`, bump a `ticks` counter under a lock, call `yield()`
      if a process is running.
- [ ] **Kernel-mode traps must save and restore `sepc`/`sstatus`,** because `yield()`
      from inside the handler switches away and other traps happen in between.
      `kerneltrap.S` currently only saves GPRs. Save `sepc` and `sstatus` in the C
      handler and restore before `sret`.
- [ ] **Turn interrupts on** in the scheduler loop (bug 2 fixed) and in `utrapret`
      (`SPIE`). Audit every `lock()` site that can now be interrupted.
- [ ] **`sleep(chan, lock)` / `wakeup(chan)`** in `proc.c`, xv6 semantics. Convert
      `wait()` to sleep on the parent, and `exit()` to `wakeup(parent)`.
- [ ] **PLIC.** `kernel/plic.c`: set priority for UART IRQ 10, enable it for each hart's
      S-context, set threshold 0; `plic_claim`/`plic_complete`. PLIC base `0x0C000000`.
- [ ] **UART rewrite** for NS16550A: `kernel/uart.c` with `LSR` polling for TX,
      a TX ring buffer drained on THR-empty interrupt, RX interrupt pushing into a
      console line buffer. `putchar` used by `printf` stays synchronous (polling) so
      panics always print.
- [ ] **Console layer** `kernel/console.c`: line editing (backspace, ctrl-U), echo,
      `consoleread`/`consolewrite` with `sleep` on the input buffer.
- [ ] `sys_read(fd=0)` and `sys_write(fd=1|2)` route to the console.

Acceptance:
- A user program that runs `while(1);` after `fork()` does not stop the parent from
  printing every tick.
- `init` reads a line from the console and echoes it back.
- `-smp 4` stress: 50 boots, no panic, no lost characters.

### M2 — Complete process lifecycle and memory syscalls (≈2 weeks)

Goal: every process-related syscall does what its name says; user faults are contained.

Tasks:
- [ ] Locking: `kmem.lock` around the bitmap; `pid_lock`; take `p->lock` in
      `procalloc`; `wait_lock` for parent/child edges (xv6 pattern). Remove the
      "reparent to pid 1" scan in `sys_exit` in favour of a proper `reparent()`.
- [ ] `kill(pid)`: set `p->killed`; check it on syscall entry/exit and when waking from
      `sleep`. `exit()` on the way back to user mode.
- [ ] `sbrk(n)`: `uvmalloc`/`uvmdealloc` in `vm.c`; grow/shrink `p->sz`. Make
      `uvmcopy` and `uvmfree` walk `[0x1000, sz)` consistently (they already do; add
      the heap semantics). Decide on a user layout and write it in `vm.h`:
      `0x0000_1000` text/data/heap growing up, guard page, stack at `USER_STACK_TOP`,
      trapframe and trampoline pages at the top (currently the trampoline is
      identity-mapped in the user table; keeping it identity-mapped is fine for rv32
      because the kernel lives at `0x8000_0000` and user code never reaches it).
- [ ] User exceptions: in `u_trap_handle`, any `scause` other than `ecall` prints a
      one-line diagnostic and kills the process instead of `error()`ing and returning
      to the faulting instruction.
- [ ] Multi-page program loading: extend `uvmfirst` into `loadseg(pt, va, src, len)`
      that maps as many pages as needed. This is the building block `exec` needs.
- [ ] `exec(path, argv)` with an in-kernel program table: `tools/bin2c.py` embeds
      several user binaries, `kernel/progs.c` maps `"name" -> {data, len}`. Build the
      argv strings and pointer array on the new stack (the comment block in
      `syscall.c:85–116` is the correct recipe; implement it as written).
- [ ] Syscall argument validation: `argaddr` checks the pointer lies below `p->sz`
      or inside the stack page; `fetchstr` for NUL-terminated user strings.

Acceptance:
- `init` forks and `exec`s `hello`; `hello` prints its argv and exits with status 3;
  `init` gets 3 back from `wait`.
- A program that dereferences NULL is killed with "pid N: page fault at 0x0"; the
  shell keeps running.
- A program that `sbrk`s 1 MB, touches it, and frees it does not leak (`kalloc`
  free-page count is the same before and after).

### M3 — User-space runtime, shell, utilities (≈2 weeks)

Goal: a person can sit at the console and use the system.

Tasks:
- [ ] `user/crt0.S` (`_start`: call `main(argc, argv)`, then `exit(ret)`).
- [ ] `user/usys.S` generated from a table: one stub per syscall (`li a7, N; ecall; ret`).
- [ ] `user/ulib.c`: `strlen`, `strcpy`, `strcmp`, `memset`, `memmove`, `atoi`,
      `gets`, `printf` (over `write`), `malloc`/`free` (over `sbrk`, K&R style).
- [ ] `user/user.ld`: text at `0x1000`, data/bss after, entry `_start`. Each program is
      its own ELF; for now stripped to a flat binary and embedded.
- [ ] `user/init.c`: open console as fd 0/1/2 (once M4 exists; until then the kernel
      pre-wires them), loop: `fork` → `exec("sh")` → `wait`.
- [ ] `user/sh.c`: read line, split on spaces, builtins `cd`/`exit`, external commands
      via `fork`+`exec`+`wait`; `&` background; then `|` and `<`/`>` once pipes and
      files exist (M4).
- [ ] `user/echo.c`, `user/hello.c`, `user/forktest.c`, `user/loop.c` (for preemption
      tests), `user/usertests.c` (grows with every milestone).
- [ ] Switch the loader from flat binaries to **ELF**: parse `Elf32_Ehdr`/`Phdr`,
      load each `PT_LOAD` segment with `loadseg`, zero-fill `memsz > filesz`. Drop the
      `objcopy -O binary` step. Entry point comes from the header instead of a fixed
      `0x1000`.

Acceptance: boot → `$` prompt → `echo hi` → `hi`. `usertests` covers fork/exec/wait/
kill/sbrk/preemption and passes under `make test`.

### M4 — Disk, filesystem, file descriptors, pipes (≈3–4 weeks)

Goal: programs live on a disk image, not inside the kernel. This is the largest
milestone; split it into three PRs.

**M4a — virtio-blk and buffer cache**
- [ ] `kernel/virtio_disk.c`: virtio MMIO legacy device at `0x10001000` (QEMU
      `-drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0`),
      IRQ 1 on the PLIC. Descriptor/avail/used rings, one request at a time is enough.
- [ ] `kernel/sleeplock.c`: sleep-locks (spinlock + `sleep`/`wakeup`).
- [ ] `kernel/bio.c`: buffer cache (`bread`/`bwrite`/`brelse`), LRU list, ~30 buffers.
- Acceptance: kernel reads block 0, prints a magic number written there by `mkfs`.

**M4b — on-disk filesystem**
- [ ] Choose the xv6 layout (boot | superblock | log | inodes | bitmap | data). It is
      simple, documented, and matches the rest of this kernel's lineage. Skip the log
      at first (`begin_op`/`end_op` as no-ops) and add crash-consistency later
      (section 4).
- [ ] `kernel/fs.c`: `ialloc`/`iget`/`ilock`/`iput`, `bmap`, `readi`/`writei`,
      `dirlookup`/`dirlink`, `namei`/`nameiparent`.
- [ ] `tools/mkfs.c`: host program that builds `fs.img` from the `user/` ELFs and a
      `README`. Add to `Makefile`.
- Acceptance: kernel `namei("/sh")` returns an inode of the right size.

**M4c — file descriptors, pipes, syscalls**
- [ ] `kernel/file.c`: `struct file` table, `filealloc`/`filedup`/`fileclose`/
      `fileread`/`filewrite`; device switch so `/console` is a device inode.
- [ ] `kernel/pipe.c`: ring buffer with `sleep`/`wakeup` on both ends.
- [ ] Syscalls: `open`, `close`, `read`, `write`, `dup`, `pipe`, `fstat`, `mkdir`,
      `mknod`, `chdir`, `link`, `unlink`. Move `exec` to load from the filesystem;
      delete the embedded program table.
- [ ] Utilities: `ls`, `cat`, `mkdir`, `rm`, `wc`, `grep` (simple).
- Acceptance: `ls | wc`, `cat README > copy; cat copy`, `mkdir d; cd d; echo x > f;
  ls`. Definition-of-done items 3 and 4 satisfied.

### M5 — Hardening and SMP correctness (≈2 weeks)

Goal: the definition of done holds under stress, on 4 CPUs, in CI.

- [ ] Lock-ordering document in `docs/LOCKING.md`; assert with `holding()` in the
      places the order matters.
- [ ] Interrupt-safety audit: every spinlock taken in interrupt context is only ever
      taken with interrupts off (`push_off` already does this; verify no bare
      `atomic_swap` paths remain).
- [ ] `usertests`: add the xv6 stress cases (`forkfork`, `sbrkmuch`, `pipe1`,
      `preempt`, `exitwait`, `mem`, `bigfile`, `manywrites`).
- [ ] Run the suite 20× on `-smp 4` in CI (`make test SMP=4 REPEAT=20`).
- [ ] `kfree` should stop zeroing pages; zero on `kalloc` instead, and add a
      "poison" fill in debug builds.
- [ ] Free-page and proc-slot accounting printed by a `sys_info` debug syscall so
      leaks are visible.
- [ ] Remove dead code (`gpr.c` mostly, commented blocks), fix warnings, build with
      `-Werror`.

Acceptance: CI green on `-smp 1` and `-smp 4`, 20 repetitions each.

---

## 3. Optional: running on physical hardware

### 3.1 The constraint that decides everything

This kernel needs an RV32 core with **supervisor mode and an Sv32 MMU** (page tables,
`satp`, `sfence.vma`), plus a CLINT-style timer and a PLIC-style interrupt controller.

Almost no commercial 32-bit RISC-V silicon has an MMU. The common boards are all
microcontrollers without S-mode:

| Chip / board | ISA | MMU | Verdict |
|---|---|---|---|
| ESP32-C3 / C6 / H2 | rv32imc | no (PMP only) | Not usable without a rewrite to a no-MMU kernel |
| GD32VF103 (Longan Nano), CH32V | rv32imac | no | Same |
| Kendryte K210, Allwinner D1, StarFive JH7110, SpacemiT K1 | rv64 | yes (Sv39) | Usable only after a 64-bit port |
| FPGA soft cores (VexRiscv, CVA6/CV32A6, Rocket rv32) | rv32ima | yes (Sv32) | **The realistic target** |

So there are two honest paths:

- **Path A (recommended): FPGA soft core with an Sv32 MMU.** Keep the OS 32-bit as
  designed. The hardware is a board plus a bitstream you generate.
- **Path B: port to rv64 and run on a cheap Linux-class SBC** (Milk-V Duo ≈ $5,
  VisionFive 2, Lichee RV). This is a real port: Sv39 three-level page tables, 64-bit
  registers in every trap/context file, new UART/clock drivers, boot via OpenSBI+U-Boot.
  Expect 2–3 weeks. Worth considering after M5 if hardware matters more than the
  "32-bit" part of the goal.

The rest of this section is Path A.

### 3.2 Recommended target: LiteX + VexRiscv-SMP

- **Core:** VexRiscv in its "Linux" / SMP configuration: rv32ima, S-mode, Sv32 MMU,
  CLINT and PLIC blocks compatible with what Linux expects. This is the same stack the
  Linux-on-LiteX project uses, so the peripherals are known to work with an
  xv6-shaped kernel.
- **SoC generator:** LiteX. It produces a memory map (`csr.h`/`soc.h`) and a BIOS that
  can load `kernel.bin` over serial or from an SD card.
- **Boards (pick one):**
  - Digilent Arty A7-35T/100T (Xilinx Artix-7, Vivado free edition). Most documented.
  - Colorlight i5 / i9 or ULX3S (Lattice ECP5, fully open toolchain: yosys, nextpnr,
    trellis). Cheapest, no vendor tools.
  - Digilent Nexys Video, or any board LiteX supports with ≥64 MB DRAM and a UART.
- **Simulation first:** `litex_sim --cpu-type=vexriscv_smp --with-sdram` runs the SoC
  under Verilator on your desktop. You can boot and debug the kernel on the exact
  peripheral set before buying anything.

### 3.3 What in the kernel has to change

Very little of the design; mostly constants and two drivers.

| Item | Today (QEMU virt) | LiteX/VexRiscv | Work |
|---|---|---|---|
| RAM base / kernel load | `0x8000_0000`, 128 MB | `0x4000_0000`, board-dependent size | Make `KERNBASE`, `PHYSTOP` come from `platform.h`; linker script takes them via `-defsym` |
| UART | NS16550A at `0x1000_0000` | LiteUART CSRs (`rxtx`, `txfull`, `rxempty`, `ev_pending`) | A second ~80-line UART driver; select at build time |
| CLINT | `0x0200_0000`, 10 MHz | LiteX `clint` block, `sys_clk`-derived rate | Constants only |
| PLIC | `0x0C00_0000`, UART IRQ 10 | LiteX `plic` block, different IRQ numbers | Constants only |
| Disk | virtio-blk | SD card via LiteSDCard, or SPI flash | New block driver behind the same `bread`/`bwrite` interface |
| Entry | QEMU jumps to ELF entry in M-mode | LiteX BIOS jumps to `kernel.bin` at RAM base in M-mode | `objcopy -O binary`; nothing else |

### 3.4 Steps

- **H0 (do during M0/M1): platform abstraction.** Create `kernel/platform.h` and move
  every hard-coded address (`UART_0`, CLINT, PLIC, `KERNBASE`, `PHYSTOP`, timer
  frequency, IRQ numbers) into it. `PLATFORM ?= qemu-virt` in the Makefile selects
  `platform/qemu-virt.h` or `platform/litex.h`. This costs an hour now and makes the
  port a configuration change later.
- **H1 (after M1): boot under OpenSBI too.** Add `make run BIOS=opensbi`
  (`-bios default`, kernel linked at `0x8020_0000`). Under a firmware the kernel starts
  in S-mode: skip `boot.c`'s M-mode setup, get the timer via SBI `set_timer` (or the
  Sstc `stimecmp` CSR), get hart id from `a0`. Keep both paths behind `#ifdef
  BOOT_MMODE`. Most real boards ship with OpenSBI, so this halves the porting risk.
- **H2 (after M4): drivers behind interfaces.** Ensure `console.c` calls a `uart_*`
  API and `bio.c` calls a `disk_*` API, so a LiteUART and an SD driver drop in.
- **H3: simulate.** Install LiteX, generate the VexRiscv-SMP SoC, boot `kernel.bin` in
  `litex_sim`. Fix whatever the different memory map exposes. No hardware needed.
- **H4: real board.** Build the bitstream, `litex_term --kernel kernel.bin /dev/ttyUSB0`
  to load over serial, then move to SD card boot. Budget: board $50–$300, one weekend
  for toolchain setup, another for the first successful boot.

---

## 4. Extensions after the core is done

Not required for "done"; listed so the core design does not paint itself into a corner.

- Crash-consistent filesystem via the write-ahead log (`begin_op`/`end_op`), then
  `fsck` in `tools/`.
- Copy-on-write `fork` and lazy `sbrk` (page-fault driven), which the M2 page-fault
  path is already shaped for.
- `mmap`/`munmap` of files.
- Signals (at least `SIGKILL`/`SIGINT` from ctrl-C on the console).
- `virtio-net` and a minimal UDP/ARP stack.
- Sstc timer extension to drop the M-mode timer trampoline entirely.
- Multiple user page-table layouts (put the trampoline at the top of VA like xv6
  so user programs can use the full low 2 GB).
- A `gdb` Python helper to walk `proctable` and page tables.

---

## 5. Suggested order for the first two weeks

Day-by-day for M0 and the start of M1, because that is where the current code is
most fragile.

1. `.gitignore`, README, `CROSS` variable, `make run`, `tools/bin2c.py`. Commit.
2. Fix `interrupt_on`, the `1UL << 32` constants, `strlen`, `r_t0`, `memcpy`, `NCPU`
   sizing. Build with `-Werror`. Commit.
3. Add `kernel_hartid` to the trapframe and reload `tp` in `usertrap.S`. Loop
   `make run SMP=4` 50 times. Commit when it never panics.
4. `panic()`; replace fatal `error()`s. `tools/run_test.sh`; `make test`. Commit.
5. GitHub Actions workflow. Commit. (M0 done.)
6. `platform.h` with QEMU virt constants (H0). Commit.
7. `timervec.S` + CLINT setup + `ticks` counter; prove a tick fires by printing every
   100 ticks. Commit.
8. `kerneltrap` saves/restores `sepc`/`sstatus`; timer → `yield()`. Prove two
   busy-looping children alternate. Commit.
9. `sleep`/`wakeup`; rewrite `wait`/`exit`. Commit.
10. NS16550A driver, PLIC, console RX. Echo a typed line. Commit. (M1 done.)

---

## 6. Working conventions

- One milestone = one branch = one PR into `main`, with `make test` green.
- Keep the layout: `kernel/` (kernel), `user/` (programs and libc), `tools/` (host
  tools), `docs/` (this file, memory map, locking rules), `platform/` (board headers).
- Every new subsystem gets a test in `user/usertests.c` or a kernel self-test behind
  `#ifdef KTEST` before the PR merges.
- Write the memory map and lock order down in `docs/` whenever they change; those two
  documents prevent most kernel bugs.
