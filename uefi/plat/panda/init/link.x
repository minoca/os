ENTRY(_start)
SECTIONS {
    .text : { *(.text) }

    . = ALIGN(4);
    .got : { *(.got) }

    . = ALIGN(4);
    .rodata : { *(.rodata) }

    . = ALIGN(4);
    .data : { *(.data) }

    . = ALIGN(4);
    __bss_start = .;
    .bss : { *(.bss) }
    . = ALIGN(4);
    __bss_end = .;
}
