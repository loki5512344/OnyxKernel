/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — virtual memory manager (Sv39).
 * Kernel uses identity mapping (vaddr == paddr) for its own region.
 * User processes get their own root page table.
 */
#ifndef SLIPPER_VMM_H
#define SLIPPER_VMM_H

#include "types.h"

/* VMA permission flags (compatible with PTE bits) */
#define VMM_R   (1U << 1)
#define VMM_W   (1U << 2)
#define VMM_X   (1U << 3)
#define VMM_U   (1U << 4)
#define VMM_G   (1U << 5)
#define VMM_RWX (VMM_R | VMM_W | VMM_X)

/* Kernel VAs are identity-mapped. */
#define KERNEL_BASE   0x80200000UL
#define KERNEL_END    ((usize)&__kernel_end)
#define MMIO_BASE     0x00000000UL
#define USER_BASE     0x00010000UL
#define USER_TOP      0x40000000UL
#define USER_STACK    (USER_TOP - PAGE_SIZE)

extern char __kernel_end[];

void vmm_init(void);

int  vmm_map(u64 *root_satp_pa, vaddr_t vaddr, paddr_t paddr,
             usize size, u32 flags);

int  vmm_map_anon(u64 *root_satp_pa, vaddr_t vaddr, usize size, u32 flags);

paddr_t vmm_new_root(void);

void vmm_install_root(paddr_t root_pa);

paddr_t vmm_translate(paddr_t root_pa, vaddr_t vaddr);

void vmm_destroy_root(paddr_t root_pa);

paddr_t vmm_kernel_root(void);

int  vmm_map_kernel(paddr_t paddr, usize size, u32 flags);

#endif
