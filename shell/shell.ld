ENTRY(main)
SECTIONS {
    . = 0x400000;       /* adresse standard des programmes */
    .text   : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) }
}