/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — base fixed-width types and macros.
 * Freestanding, no libc.
 */
#ifndef SLIPPER_TYPES_H
#define SLIPPER_TYPES_H

typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long       u64;
typedef unsigned long       usize;
typedef unsigned long       uptr;
typedef unsigned long       paddr_t;
typedef unsigned long       vaddr_t;

typedef enum { false = 0, true = 1 } bool;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define KB(n) ((usize)(n) * 1024UL)
#define MB(n) ((usize)(n) * 1024UL * 1024UL)
#define GB(n) ((usize)(n) * 1024UL * 1024UL * 1024UL)

#define PAGE_SIZE   4096UL
#define PAGE_SHIFT  12
#define PAGE_MASK   (PAGE_SIZE - 1)

#define PAGE_ALIGN_DOWN(x)  ((usize)(x) & ~PAGE_MASK)
#define PAGE_ALIGN_UP(x)    (((usize)(x) + PAGE_MASK) & ~PAGE_MASK)
#define IS_ALIGNED(x, a)    (((usize)(x) & ((usize)(a) - 1)) == 0)

#define ARR_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define unlikely(x)   __builtin_expect(!!(x), 0)
#define likely(x)     __builtin_expect(!!(x), 1)

/* Error codes (NOT errno-compatible, our own namespace). */
#define SL_OK            0
#define SL_ERR_NOMEM    -1
#define SL_ERR_INVAL    -2
#define SL_ERR_NOENT    -3
#define SL_ERR_IO       -4
#define SL_ERR_PERM     -5
#define SL_ERR_RANGE    -6
#define SL_ERR_NOSYS    -7
#define SL_ERR_BUSY     -8

#endif
