.section .text.boot
.globl _start

_start:
    bnez    a0, _hang

    la      t0, _sbss
    la      t1, _ebss
    li      t2, 0
1:
    bgeu    t0, t1, 2f
    sw      t2, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    la      sp, _stack_end

    call    kernel_main

_hang:
    wfi
    j       _hang
