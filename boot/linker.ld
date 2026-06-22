OUTPUT_ARCH(riscv)
ENTRY(_start)

BASE_ADDRESS = 0x80000000;

SECTIONS
{
    . = BASE_ADDRESS;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        _sbss = .;
        *(.bss .bss.*)
        _ebss = .;
    }

    . = ALIGN(16);
    _stack_start = .;
    . += 0x4000;
    _stack_end = .;

    _end = .;
}
