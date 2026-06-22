// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — kernel logging implementation.
 */
#include <stdarg.h>
#include "types.h"
#include "klog.h"
#include "uart.h"
#include "riscv.h"

static int g_klog_level = KLOG_LEVEL;

static void emit_str(const char *s) { klog_puts(s); }
static void emit_char(char c) { klog_putc(c); }

void klog_raw(const char *s) { uart_puts(&g_uart, s); }
void klog_putc(char c)       { uart_putc(&g_uart, c); }

void klog_puts(const char *s) { uart_puts(&g_uart, s); }

void klog_hex(u64 v, int width)
{
    static const char *hexd = "0123456789abcdef";
    char buf[17];
    int i = 16;
    buf[16] = 0;
    if (v == 0) {
        if (width <= 1) { emit_char('0'); return; }
        for (int j = 0; j < width - 1; ++j) emit_char('0');
        emit_char('0');
        return;
    }
    while (v && i > 0) {
        buf[--i] = hexd[v & 0xF];
        v >>= 4;
    }
    if (width > 0) {
        int pad = width - (16 - i);
        for (int j = 0; j < pad; ++j) emit_char('0');
    }
    emit_str(&buf[i]);
}

void klog_dec(i64 v)
{
    char buf[24];
    int i = 23;
    buf[i] = 0;
    bool neg = v < 0;
    u64 uv = neg ? (u64)(-(v + 1)) + 1 : (u64)v;
    if (uv == 0) { emit_char('0'); return; }
    while (uv) {
        buf[--i] = '0' + (char)(uv % 10);
        uv /= 10;
    }
    if (neg) emit_char('-');
    emit_str(&buf[i]);
}

static void klog_str_impl(const char *s) { emit_str(s); }

static void vprintf(const char *fmt, va_list ap)
{
    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') { emit_char(*p); continue; }
        ++p;
        if (!*p) break;
        int width = 0;
        bool lz = false;
        if (*p == '0') { lz = true; ++p; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); ++p; }
        /* Skip length modifiers (l, ll, z) — no-op on RV64 */
        if (*p == 'l' || *p == 'z') { ++p; }
        if (!*p) break;
        switch (*p) {
        case 's': emit_str(va_arg(ap, const char*)); break;
        case 'c': emit_char((char)va_arg(ap, int));  break;
        case 'd': klog_dec((i64)va_arg(ap, long));   break;
        case 'u': {
            u64 v = (u64)va_arg(ap, unsigned long);
            char buf[24]; int i = 23; buf[i] = 0;
            if (!v) emit_char('0');
            while (v) { buf[--i] = '0' + (char)(v % 10); v /= 10; }
            emit_str(&buf[i]);
            break;
        }
        case 'x': klog_hex((u64)va_arg(ap, unsigned long), lz ? (width>0?width:0) : 0); break;
        case 'p': emit_str("0x"); klog_hex((u64)va_arg(ap, unsigned long), 16); break;
        case '%': emit_char('%'); break;
        default:  emit_char('%'); emit_char(*p); break;
        }
    }
}

void _klog_emit(int level, const char *tag, const char *fmt, ...)
{
    if (level > g_klog_level) return;
    emit_char('[');
    emit_str(tag);
    emit_str("] ");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    emit_char('\n');
}

__attribute__((noreturn))
void kpanic(const char *fmt, ...)
{
    /* Disable interrupts. */
    csr_clear(sstatus, SSTATUS_SIE);

    emit_str("\n[PANIC] ");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    emit_char('\n');

    emit_str("  sepc   = 0x"); klog_hex(csr_read(sepc), 16); emit_char('\n');
    emit_str("  sstatus= 0x"); klog_hex(csr_read(sstatus), 16); emit_char('\n');
    emit_str("  scause = 0x"); klog_hex(csr_read(scause), 16); emit_char('\n');
    emit_str("  stval  = 0x"); klog_hex(csr_read(stval), 16); emit_char('\n');
    emit_str("  ra     = 0x"); klog_hex((u64)__builtin_return_address(0), 16); emit_char('\n');
    emit_str("  sp     = 0x"); klog_hex((u64)__builtin_frame_address(0), 16); emit_char('\n');

    khalt();
}

__attribute__((noreturn))
void khalt(void)
{
    for (;;) {
        csr_clear(sstatus, SSTATUS_SIE);
        asm volatile("wfi");
    }
}
