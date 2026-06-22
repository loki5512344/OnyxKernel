/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — linker script.
 *
 * SlipperBoot loads kernel.elf at 0x80200000 (above the 0x80000000 bootloader
 * region). The kernel runs in S-mode after the M-mode stub in boot.S does
 * the mret.
 *
 * Memory map (QEMU virt, 256M default):
 *   0x00000000 .. 0x0FFFFFFF  MMIO window (UART, virtio, CLINT, PLIC)
 *   0x02000000                CLINT
 *   0x0C000000                PLIC
 *   0x10000000                NS16550A UART
 *   0x10001000                virtio,mmio (first)
 *   0x10008000                virtio,mmio (second, if used)
 *   0x80000000                DRAM base, SlipperBoot lives here (32K)
 *   0x80200000                SlipperKernel entry
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

KERNEL_BASE = 0x80200000;

SECTIONS
{
    . = KERNEL_BASE;

    .text : {
        KEEP(*(.text.boot))
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

    . = ALIGN(8);
    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
    }
    . = ALIGN(8);
    __bss_end = .;

    /* Kernel stack: 16K, grows down from __stack_top. */
    . = ALIGN(16);
    __stack_bottom = .;
    . = . + 16K;
    . = ALIGN(4096);
    __stack_top = .;

    /* End of kernel image (incl. stack). PMM bitmap and heap start here. */
    __kernel_end = .;

    /DISCARD/ : {
        *(.note .note.*)
        *(.comment)
        *(.eh_frame .eh_frame_hdr)
        *(.riscv.attributes)
    }
}
