/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — kernel heap (free-list allocator on top of PMM).
 */
#ifndef SLIPPER_HEAP_H
#define SLIPPER_HEAP_H

#include "types.h"

void *kmalloc(usize size);
void *kmalloc_aligned(usize size, usize align);
void  kfree(void *p);
void *krealloc(void *p, usize new_size);
void  heap_init(void);

/* Bulk allocators for page-sized things. */
paddr_t heap_alloc_page(void);
void    heap_free_page(paddr_t pa);

/* Statistics for /proc-ish views later. */
usize heap_used(void);
usize heap_free(void);

#endif
