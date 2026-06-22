/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — physical memory manager (4K bitmap).
 */
#ifndef SLIPPER_PMM_H
#define SLIPPER_PMM_H

#include "types.h"

void  pmm_init(paddr_t dram_base, usize dram_size);
paddr_t pmm_alloc(void);           /* one 4K page, 0 on OOM */
paddr_t pmm_alloc_n(usize pages);  /* physically contiguous, 0 on OOM */
void  pmm_free(paddr_t pa);
void  pmm_free_n(paddr_t pa, usize pages);
void  pmm_reserve(paddr_t pa, usize pages);
usize pmm_free_pages(void);
usize pmm_total_pages(void);

/* Convenience: allocate a zeroed page. */
paddr_t pmm_alloc_zero(void);

#endif
