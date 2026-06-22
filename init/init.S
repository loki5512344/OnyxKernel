/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — sample /bin/init in SlipperExec format.
 *
 * This is the very first user-space program run by SlipperKernel.
 * It writes a banner to stdout via SYS_write and exits via SYS_exit.
 *
 * SlipperExec binaries do NOT use libc and do NOT assume any particular
 * stack layout beyond argc/argv. In MVP the kernel passes argc=0.
 */

    .section .text.start, "ax"
    .global _start
_start:
    /* SYS_write(1, msg, 33) */
    li      a7, 1
    li      a0, 1
    la      a1, msg
    li      a2, 33
    ecall

    /* SYS_exit(0) */
    li      a7, 3
    li      a0, 0
    ecall

    /* Should never reach here. */
1:
    wfi
    j       1b

    .section .rodata
msg:
    .asciz "Hello from SlipperOS /bin/init!\n"
