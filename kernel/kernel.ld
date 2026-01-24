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