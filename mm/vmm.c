// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — Sv39 paging — management (root alloc, install, destroy, init).
 *
 * Kernel uses identity mapping (vaddr == paddr).
 * User processes get their own root via vmm_new_root().
 */
#include "types.h"
#include "vmm.h"
#include "pmm.h"
#include "klog.h"
#include "riscv.h"

u64 *g_kernel_root = NULL;

u64 g_kernel_root_satp = 0;

void free_subtree(u64 *tbl, int lv);

paddr_t vmm_new_root(void)
{
    return pmm_alloc_zero();
}

void vmm_install_root(paddr_t root_pa)
{
    u64 satp_val = SATP_MODE_SV39 | ((u64)root_pa >> PAGE_SHIFT);
    asm volatile("csrw satp, %0\nsfence.vma" : : "r"(satp_val) : "memory");
}

paddr_t vmm_kernel_root(void)
{
    return (paddr_t)g_kernel_root;
}

void vmm_destroy_root(paddr_t root_pa)
{
    free_subtree((u64 *)root_pa, SV39_LEVELS - 1);
    pmm_free(root_pa);
}

int vmm_map_kernel(paddr_t paddr, usize size, u32 flags)
{
    return vmm_map(g_kernel_root, (vaddr_t)paddr, paddr, size, flags);
}

void vmm_init(void)
{
    paddr_t root = pmm_alloc_zero();
    if (!root) kpanic("vmm: no memory for root page table");
    g_kernel_root = (u64 *)root;

    u32 rwx = VMM_R | VMM_W | VMM_X;
    u64 pte_flags = PTE_V | rwx | PTE_A | PTE_D;

    u64 ppn0 = 0UL >> 12;
    u64 ppn1 = 0x40000000UL >> 12;
    u64 ppn2 = 0x80000000UL >> 12;
    g_kernel_root[0] = (ppn0 << PTE_PPN_SHIFT) | pte_flags;
    g_kernel_root[1] = (ppn1 << PTE_PPN_SHIFT) | pte_flags;
    g_kernel_root[2] = (ppn2 << PTE_PPN_SHIFT) | pte_flags;

    vmm_install_root((paddr_t)g_kernel_root);
    g_kernel_root_satp = SATP_MODE_SV39 | ((u64)(paddr_t)g_kernel_root >> PAGE_SHIFT);

    kinf("vmm: Sv39 on, kernel root @0x%lx", (u64)(paddr_t)g_kernel_root);
}
