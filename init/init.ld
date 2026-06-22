/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — linker script for /bin/init (SlipperExec target).
 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x10000;
    .text : {
        *(.text.start)
        *(.text .text.*)
    }
    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    }
    .bss : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
    }
    /DISCARD/ : {
        *(.eh_frame)
        *(.riscv.attributes)
        *(.comment)
        *(.note .note.*)
    }
}
