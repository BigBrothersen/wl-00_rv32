/* MEMORY STRUCTURE:
|   stack   |
|     ↓     |
|     ↑     |
|   heap    |
|    bss    |
|   data    |
|   text    |

ALIGN(16) -> align address to the multiple of 16 (0x10)
*/

/* MEMORY STRUCTURE (Highest Address to Lowest):
|           |   
|   heap    |   → Starts at 'end'. Grows ↑ towards max physical RAM.
|-----------|   → end (Aligned to 0x1000)
|   stack   |   → 16KB (4 CPUs * 4096B). Grows ↓ from stack_bottom.
|-----------|   → stack_top
| guard page|   → 4KB empty space. Catches stack overflows before they hit .bss!
|-----------|
|    bss    |   → Uninitialized global variables.
|-----------|
|   data    |   → Initialized global variables.
|-----------|   → _etext
| trampoline|   → Aligned to 0x1000. Critical for user-mode trap routing.
|   text    |   → 0x80000000 (QEMU default load address). Size = _etext - _stext
*/

ENTRY(_entry)
SECTIONS
{
    . = 0x80000000;

    _stext = .; 
    .text : {
        *(.text .text.*)
        . = ALIGN(0x1000); 
        _trampoline = .; 
        *(.trampoline_text)
        . = ALIGN(0x1000); 
        /* ASSERT(. - _trampoline == 0x1000, "Error: trampoline larger than one page"); */
    }
    
    _etext = .;

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
    }

    . = ALIGN(0x1000);
    . = . + 0x1000;
    stack_top = .;

    . = . + (4096 * 4); /* 4 CPU */

    stack_bottom = .;
    
    . = ALIGN(0x1000);
    PROVIDE(end = .);
}