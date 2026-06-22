/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — minimal NS16550A UART driver.
 * Same protocol as SlipperBoot; reg-shift comes from FDT.
 */
#ifndef SLIPPER_UART_H
#define SLIPPER_UART_H

#include "types.h"

#define UART_DATA     0x00
#define UART_IER      0x01
#define UART_IIR_FCR  0x02
#define UART_LCR      0x03
#define UART_MCR      0x04
#define UART_LSR      0x05
#define UART_DLL      0x00
#define UART_DLM      0x01

#define LSR_THRE      0x20
#define LSR_DR        0x01

typedef struct {
    uptr base;
    u32  reg_shift;   /* 0 = byte, 1 = u16, 2 = u32 */
} uart_t;

void uart_init(uart_t *u, uptr base, u32 reg_shift);
void uart_putc(uart_t *u, char c);
void uart_puts(uart_t *u, const char *s);
int  uart_getc(uart_t *u);     /* -1 if no input */

extern uart_t g_uart;

#endif
