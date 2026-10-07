ENTRY(start)

/* Two loadable segments with different permissions. Without PHDRS the
   linker merges everything into one read+write+execute segment. */
PHDRS
{
    text PT_LOAD FLAGS(5);      /* R + X: code and read-only data */
    data PT_LOAD FLAGS(6);      /* R + W: writable data and .bss */
}

SECTIONS
{
    . = 0x1000;                 /* USER_BASE: page 0 stays unmapped */
    .text : { *(.text .text.*) } :text
    .rodata : { *(.srodata .srodata.*) *(.rodata .rodata.*) } :text
    . = ALIGN(0x1000);          /* data on its own page: different permissions */
    .data : { *(.sdata .sdata.*) *(.data .data.*) } :data
    .bss : { *(.sbss .sbss.*) *(.bss .bss.*) *(COMMON) } :data

    PROVIDE(end = .);
}
