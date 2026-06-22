// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — NS16550A UART driver.
 */
#include "types.h"
#include "uart.h"
#include "riscv.h"

uart_t g_uart = { 0x10000000, 2 };   /* defaults for QEMU virt: u32 stride */

static inline volatile u8 *reg(uptr base, u32 shift, int off)
{
    return (volatile u8 *)(base + ((usize)off << shift));
}

void uart_init(uart_t *u, uptr base, u32 reg_shift)
{
    u->base = base;
    u->reg_shift = reg_shift;

    /* Disable interrupts. */
    *reg(base, reg_shift, UART_IER) = 0x00;
    /* Enable DLAB to set baud divisor. */
    *reg(base, reg_shift, UART_LCR) = 0x80;
    /* Divisor 1 -> 115200 baud on QEMU (where input clock is 3.6864 MHz).
     * On real boards the divisor may need adjustment via FDT clock info. */
    *reg(base, reg_shift, UART_DLL) = 0x01;
    *reg(base, reg_shift, UART_DLM) = 0x00;
    /* 8N1, no parity, disable DLAB. */
    *reg(base, reg_shift, UART_LCR) = 0x03;
    /* Enable FIFO, clear them, 14-byte threshold. */
    *reg(base, reg_shift, UART_IIR_FCR) = 0xC7;
    /* RTS/DSR set, OUT2 (needed for IRQ routing on PC — harmless here). */
    *reg(base, reg_shift, UART_MCR) = 0x0B;
}

void uart_putc(uart_t *u, char c)
{
    volatile u8 *lsr = reg(u->base, u->reg_shift, UART_LSR);
    volatile u8 *data = reg(u->base, u->reg_shift, UART_DATA);
    while ((*lsr & LSR_THRE) == 0) { /* spin */ }
    *data = (u8)c;
}

void uart_puts(uart_t *u, const char *s)
{
    while (*s) {
        if (*s == '\n') uart_putc(u, '\r');
        uart_putc(u, *s++);
    }
}

int uart_getc(uart_t *u)
{
    volatile u8 *lsr = reg(u->base, u->reg_shift, UART_LSR);
    if (!(*lsr & LSR_DR)) return -1;
    return *reg(u->base, u->reg_shift, UART_DATA);
}
