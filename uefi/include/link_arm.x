OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm",
              "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
    PROVIDE (__executable_start = SEGMENT_START("text-segment", 0x400)); . = SEGMENT_START("text-segment", 0x400);
    .rel.dyn        :
    {
        *(.rel.init)
        *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
        *(.rel.fini)
        *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
        *(.rel.data.rel.ro .rel.data.rel.ro.* .rel.gnu.linkonce.d.rel.ro.*)
        *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
        *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
        *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
        *(.rel.ctors)
        *(.rel.dtors)
        *(.rel.got)
        *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
        PROVIDE_HIDDEN (__rel_iplt_start = .);
        *(.rel.iplt)
        PROVIDE_HIDDEN (__rel_iplt_end = .);
        PROVIDE_HIDDEN (__rela_iplt_start = .);
        PROVIDE_HIDDEN (__rela_iplt_end = .);
    }

    .rela.dyn       :
    {
        *(.rela.init)
        *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
        *(.rela.fini)
        *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
        *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
        *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
        *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
        *(.rela.ctors)
        *(.rela.dtors)
        *(.rela.got)
        *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
        PROVIDE_HIDDEN (__rel_iplt_start = .);
        PROVIDE_HIDDEN (__rel_iplt_end = .);
        PROVIDE_HIDDEN (__rela_iplt_start = .);
        *(.rela.iplt)
        PROVIDE_HIDDEN (__rela_iplt_end = .);
    }

    .text           :
    {
        *(.text.unlikely .text.*_unlikely)
        *(.text.exit .text.exit.*)
        *(.text.startup .text.startup.*)
        *(.text.hot .text.hot.*)
        *(.text .stub .text.* .gnu.linkonce.t.*)
        *(.gnu.warning)
        *(.glue_7t) *(.glue_7) *(.vfp11_veneer) *(.v4_bx)
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    _edata = .; PROVIDE (edata = .);
    .data           :
    {
        __data_start = . ;
        *(.interp)
        *(.got.plt) *(.igot.plt) *(.got) *(.igot)
        *(.data .data.* .gnu.linkonce.d.*)
        *(.dynbss)
        __bss_start = .;
        __bss_start__ = .;
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(. != 0 ? 32 / 8 : 1);
        _bss_end__ = . ; __bss_end__ = . ;
    }

    .eh_frame       :
    {
        *(.eh_frame)
    }

    .rel.plt        :
    {
        *(.rel.plt)
    }
    .rela.plt       :
    {
        *(.rela.plt)
    }

    . = ALIGN(32 / 8);
    . = ALIGN(32 / 8);
    .hash           : { *(.hash) }
    .gnu.hash       : { *(.gnu.hash) }
    .dynsym         : { *(.dynsym) }
    .dynstr         : { *(.dynstr) }
    __end__ = . ;
    _end = .; PROVIDE (end = .);
    /* Stabs debugging sections.  */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    /* DWARF debug sections. */
    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_info 0 : { *(.debug_info) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_frame 0 : { *(.debug_frame) }
    .debug_line 0 : { *(.debug_line) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_ranges 0 : { *(.debug_ranges) }
    .debug_macinfo 0 : { *(.debug_macinfo) }
    .debug_pubtypes 0 : { *(.debug_pubtypes) }
}
