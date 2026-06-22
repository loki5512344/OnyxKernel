// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — Sv39 paging — page-table walk, map, translate, free.
 *
 * Levels (Sv39):
 *   level 2 = 1 GB pages (huge)
 *   level 1 = 2 MB pages (huge)
 *   level 0 = 4 KB pages (regular)
 */
#include "types.h"
#include "vmm.h"
#include "pmm.h"
#include "klog.h"
#include "riscv.h"

extern u64 *g_kernel_root;

static u64 *walk(u64 *root, vaddr_t va, int leaf_level, bool create)
{
    u64 *tbl = root;
    for (int lv = SV39_LEVELS - 1; lv > leaf_level; --lv) {
        usize idx = (va >> (12 + 9 * lv)) & 0x1FF;
        u64 pte = tbl[idx];
        if (pte & PTE_V) {
            if (pte & PTE_LEAF) {
                if (!create) return NULL;
                paddr_t new_tbl_pa = pmm_alloc_zero();
                if (!new_tbl_pa) return NULL;
                u64 *new_tbl = (u64 *)new_tbl_pa;
                u64 flags = pte & PTE_FLAGS_MASK;
                paddr_t base_pa = ((pte >> (9 * lv + PTE_PPN_SHIFT))
                                   << (PAGE_SHIFT + 9 * lv));
                usize step = 1UL << (12 + 9 * (lv - 1));
                for (usize i = 0; i < SV39_PTES_PT; i++) {
                    paddr_t pa = base_pa + i * step;
                    u64 entry = ((pa >> (PAGE_SHIFT + 9 * (lv - 1)))
                                 << (9 * (lv - 1) + PTE_PPN_SHIFT)) | flags;
                    new_tbl[i] = entry;
                }
                tbl[idx] = ((new_tbl_pa >> PAGE_SHIFT) << PTE_PPN_SHIFT) | PTE_V;
                tbl = new_tbl;
            } else {
                tbl = (u64 *)((pte >> PTE_PPN_SHIFT) << PAGE_SHIFT);
            }
        } else {
            if (!create) return NULL;
            paddr_t pa = pmm_alloc_zero();
            if (!pa) return NULL;
            tbl[idx] = (pa >> PAGE_SHIFT << PTE_PPN_SHIFT) | PTE_V;
            tbl = (u64 *)pa;
        }
    }
    usize idx = (va >> (12 + 9 * leaf_level)) & 0x1FF;
    return &tbl[idx];
}

static int map_one(u64 *root, vaddr_t va, paddr_t pa, u32 flags, int level)
{
    u64 *pte = walk(root, va, level, true);
    if (!pte) return SL_ERR_NOMEM;
    u64 entry = ((pa >> (PAGE_SHIFT + 9 * level)) << (9 * level + PTE_PPN_SHIFT))
                | PTE_V | (flags & PTE_FLAGS_MASK) | PTE_A | PTE_D;
    *pte = entry;
    return SL_OK;
}

static int best_level(vaddr_t va, paddr_t pa, usize remaining)
{
    if (remaining >= (1UL << 30) &&
        (va & ((1UL << 30) - 1)) == 0 &&
        (pa & ((1UL << 30) - 1)) == 0) return 2;
    if (remaining >= (1UL << 21) &&
        (va & ((1UL << 21) - 1)) == 0 &&
        (pa & ((1UL << 21) - 1)) == 0) return 1;
    return 0;
}

int vmm_map(u64 *root_satp_pa, vaddr_t vaddr, paddr_t paddr,
            usize size, u32 flags)
{
    if (size == 0) return SL_OK;
    if (size & PAGE_MASK) return SL_ERR_INVAL;
    if (vaddr & PAGE_MASK) return SL_ERR_INVAL;
    if (paddr & PAGE_MASK) return SL_ERR_INVAL;

    usize off = 0;
    while (off < size) {
        usize rem = size - off;
        int lv = best_level(vaddr + off, paddr + off, rem);
        usize step = 1UL << (12 + 9 * lv);
        int rc = map_one(root_satp_pa, vaddr + off, paddr + off, flags, lv);
        if (rc) return rc;
        off += step;
    }
    return SL_OK;
}

int vmm_map_anon(u64 *root_satp_pa, vaddr_t vaddr, usize size, u32 flags)
{
    if (size & PAGE_MASK) return SL_ERR_INVAL;
    if (vaddr & PAGE_MASK) return SL_ERR_INVAL;
    for (usize off = 0; off < size; off += PAGE_SIZE) {
        paddr_t pa = pmm_alloc_zero();
        if (!pa) return SL_ERR_NOMEM;
        int rc = map_one(root_satp_pa, vaddr + off, pa, flags, 0);
        if (rc) return rc;
    }
    return SL_OK;
}

paddr_t vmm_translate(paddr_t root_pa, vaddr_t vaddr)
{
    u64 *tbl = (u64 *)root_pa;
    for (int lv = SV39_LEVELS - 1; lv >= 0; --lv) {
        usize idx = (vaddr >> (12 + 9 * lv)) & 0x1FF;
        u64 pte = tbl[idx];
        if (!(pte & PTE_V)) return 0;
        if (pte & PTE_LEAF) {
            usize shift = 12 + 9 * lv;
            u64  mask   = (1UL << shift) - 1;
            return ((pte >> PTE_PPN_SHIFT) << PAGE_SHIFT) + (vaddr & mask);
        }
        tbl = (u64 *)((pte >> PTE_PPN_SHIFT) << PAGE_SHIFT);
    }
    return 0;
}

void free_subtree(u64 *tbl, int lv)
{
    if (lv == 0) return;
    for (usize i = 0; i < SV39_PTES_PT; ++i) {
        u64 pte = tbl[i];
        if ((pte & PTE_V) && !(pte & PTE_LEAF)) {
            u64 *child = (u64 *)((pte >> PTE_PPN_SHIFT) << PAGE_SHIFT);
            free_subtree(child, lv - 1);
            pmm_free((paddr_t)child);
        }
    }
}
