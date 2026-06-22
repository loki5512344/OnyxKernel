/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — kernel logging.
 * Levels controlled at compile time via KLOG_LEVEL.
 *   0 = none, 1 = ERR, 2 = WRN, 3 = INF, 4 = DBG
 */
#ifndef SLIPPER_KLOG_H
#define SLIPPER_KLOG_H

#include "types.h"

#ifndef KLOG_LEVEL
#define KLOG_LEVEL 3
#endif

void klog_raw(const char *s);
void klog_putc(char c);
void klog_puts(const char *s);
void klog_hex(u64 v, int width);
void klog_dec(i64 v);
void klog_str(const char *s);

void _klog_emit(int level, const char *tag, const char *fmt, ...);

#define KLOG_ERR 1
#define KLOG_WRN 2
#define KLOG_INF 3
#define KLOG_DBG 4

#if KLOG_LEVEL >= 1
#define kerr(...) _klog_emit(KLOG_ERR, "ERR", __VA_ARGS__)
#else
#define kerr(...) ((void)0)
#endif
#if KLOG_LEVEL >= 2
#define kwrn(...) _klog_emit(KLOG_WRN, "WRN", __VA_ARGS__)
#else
#define kwrn(...) ((void)0)
#endif
#if KLOG_LEVEL >= 3
#define kinf(...) _klog_emit(KLOG_INF, "INF", __VA_ARGS__)
#else
#define kinf(...) ((void)0)
#endif
#if KLOG_LEVEL >= 4
#define kdbg(...) _klog_emit(KLOG_DBG, "DBG", __VA_ARGS__)
#else
#define kdbg(...) ((void)0)
#endif

void kpanic(const char *fmt, ...) __attribute__((noreturn));
void khalt(void) __attribute__((noreturn));

#endif
