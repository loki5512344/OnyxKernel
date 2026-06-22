OUTPUT_ARCH(riscv)
ENTRY(_start)

KERNEL_BASE = 0x80200000;

PHDRS
{
    text    PT_LOAD FLAGS(5);   /* PF_R | PF_X */
    rodata  PT_LOAD FLAGS(4);   /* PF_R */
    data    PT_LOAD FLAGS(6);   /* PF_R | PF_W */
    bss     PT_LOAD FLAGS(6);   /* PF_R | PF_W */
}

SECTIONS
{
    . = KERNEL_BASE;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } :text

    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } :rodata

    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } :data

    . = ALIGN(8);
    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
    } :bss
    . = ALIGN(8);
    __bss_end = .;

    . = ALIGN(16);
    __stack_bottom = .;
    . = . + 16K;
    . = ALIGN(4096);
    __stack_top = .;

    __kernel_end = .;

    /DISCARD/ : {
        *(.note .note.*)
        *(.comment)
        *(.eh_frame .eh_frame_hdr)
        *(.riscv.attributes)
    }
}
