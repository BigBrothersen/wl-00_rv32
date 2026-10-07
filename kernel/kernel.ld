/* MEMORY STRUCTURE (Highest Address to Lowest):
|           |
|   heap    |   → Starts at 'end'. Grows ↑ towards max physical RAM (handed to kalloc).
|-----------|   → end (Aligned to 0x1000)
|   stack   |   → 16KB (NCPU * 4096B). Hart N's stack grows ↓ from stack_bottom - N*4096.
|-----------|   → stack_top
|    gap    |   → 4KB unused space between .bss and the stacks. NOTE: it is still mapped
|           |     RW in kptable, so it does not fault on overflow; it only buys slack.
|-----------|
|    bss    |   → Uninitialized globals (.bss + .sbss). Zeroed by hart 0 in boot_asm.S.
|-----------|   → _bss_start
|   data    |   → Initialized globals (.data + .sdata) and read-only data (.rodata).
|-----------|   → _etext
| trampoline|   → Aligned to 0x1000. Critical for user-mode trap routing.
|   text    |   → 0x80000000 (QEMU default load address). Size = _etext - _stext
*/

ENTRY(_entry)
SECTIONS
{
    . = 0x80000000;

    _stext = .;     /* _stext starts at 0x80000000*/
    .text : {
        *(.text.entry)      /* _entry must be the first instruction at 0x80000000 */
        *(.text .text.*)
        . = ALIGN(0x1000);
        _trampoline = .;
        *(.trampoline_text)
        . = ALIGN(0x1000);
        ASSERT(. - _trampoline == 0x1000, "Error: trampoline larger than one page");
    }

    _etext = .;     /* marks the end of the OS text*/

    .rodata : {
        *(.srodata .srodata.*)
        *(.rodata .rodata.*)
    }

    .data : {
        *(.sdata .sdata.*)
        *(.data .data.*)
    }

    /* Every zero-initialized section must sit inside [_bss_start, _bss_end),
       otherwise the linker places it after 'end' and kalloc() hands it out. */
    .bss : {
        . = ALIGN(4);
        _bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    }

    . = ALIGN(0x1000);
    . = . + 0x1000;
    stack_top = .;

    . = . + (4096 * 4); /* NCPU (defs.h) boot stacks, one page each */

    stack_bottom = .;

    . = ALIGN(0x1000);
    PROVIDE(end = .);
}
