ENTRY(main)
SECTIONS {
    . = 0x500000;       /* adresse standard des pilotes */
    .text   : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) }
}

