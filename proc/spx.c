// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — SlipperExec (.spx) loader.
 *
 * Parses an .spx image, allocates a fresh user page table, maps all
 * PT_LOAD-style segments, and allocates a 64K user stack.
 */
#include "types.h"
#include "spx.h"
#include "vmm.h"
#include "pmm.h"
#include "heap.h"
#include "klog.h"
#include "riscv.h"

#define USER_STACK_PAGES  16     /* 64 KB */

int spx_load(const void *image, usize image_size,
             vaddr_t *out_entry, paddr_t *out_root_pa, vaddr_t *out_ustack)
{
    if (image_size < sizeof(spx_header_t)) return SL_ERR_INVAL;
    const spx_header_t *h = (const spx_header_t *)image;
    if (h->magic != SPX_MAGIC) {
        kerr("spx: bad magic 0x%x", h->magic);
        return SL_ERR_INVAL;
    }
    if (h->version != SPX_VERSION) {
        kerr("spx: unsupported version %u", h->version);
        return SL_ERR_INVAL;
    }
    if (h->nsegs == 0 || h->nsegs > SPX_MAX_SEGS) {
        kerr("spx: bad nsegs %u", h->nsegs);
        return SL_ERR_INVAL;
    }

    /* Allocate user root. */
    paddr_t root = vmm_new_root();
    if (!root) return SL_ERR_NOMEM;

    /* Map kernel+MMIO into user root (without U bit) so trap handler
     * running with user satp can access kernel code/data/MMIO.
     * SUM bit in sstatus gives S-mode access to U pages. */
    u32 rwx = VMM_R | VMM_W | VMM_X;
    u64 pte_flags = PTE_V | rwx | PTE_A | PTE_D;
    u64 ppn0 = 0UL >> 12;
    u64 ppn1 = 0x40000000UL >> 12;
    u64 ppn2 = 0x80000000UL >> 12;
    u64 *urt = (u64 *)root;
    urt[0] = (ppn0 << PTE_PPN_SHIFT) | pte_flags;
    urt[1] = (ppn1 << PTE_PPN_SHIFT) | pte_flags;
    urt[2] = (ppn2 << PTE_PPN_SHIFT) | pte_flags;

    /* Map each segment. */
    for (u32 i = 0; i < h->nsegs; ++i) {
        const spx_segment_t *s = &h->segs[i];
        if (s->vaddr < USER_BASE || s->vaddr >= USER_TOP) {
            kerr("spx: seg %u vaddr 0x%lx out of user region", i, s->vaddr);
            vmm_destroy_root(root);
            return SL_ERR_RANGE;
        }
        usize filesz = (usize)s->filesz;
        usize memsz  = (usize)s->memsz;
        if (filesz > memsz) {
            vmm_destroy_root(root);
            return SL_ERR_INVAL;
        }
        if (s->offset + filesz > image_size) {
            kerr("spx: seg %u extends past image end", i);
            vmm_destroy_root(root);
            return SL_ERR_INVAL;
        }
        usize map_sz = PAGE_ALIGN_UP(memsz);
        vaddr_t vstart = PAGE_ALIGN_DOWN(s->vaddr);
        /* Allocate physical pages and copy file bytes, then zero the bss. */
        usize npages = map_sz / PAGE_SIZE;
        for (usize p = 0; p < npages; ++p) {
            paddr_t pa = pmm_alloc_zero();
            if (!pa) {
                vmm_destroy_root(root);
                return SL_ERR_NOMEM;
            }
            /* Map into user root with U bit. */
            int rc = vmm_map((u64 *)root, vstart + p * PAGE_SIZE, pa,
                             PAGE_SIZE, s->flags | VMM_U);
            if (rc) {
                vmm_destroy_root(root);
                return rc;
            }
            /* Copy file bytes that fall into this page. */
            u64 seg_off = s->vaddr + (u64)p * PAGE_SIZE;
            u64 file_off = s->offset + (seg_off - s->vaddr);
            if (seg_off < s->vaddr + filesz) {
                usize chunk = MIN(PAGE_SIZE, (usize)(s->vaddr + filesz - seg_off));
                const u8 *src = (const u8 *)image + file_off;
                u8 *dst = (u8 *)pa;
                for (usize k = 0; k < chunk; ++k) dst[k] = src[k];
            }
            /* bss (memsz - filesz) is already zero from pmm_alloc_zero. */
        }
        kinf("spx: seg %u va=0x%lx fsz=%lu msz=%lu fl=0x%x",
             i, s->vaddr, filesz, memsz, s->flags);
    }

    /* Allocate user stack. Top = USER_TOP - PAGE, grows down. */
    vaddr_t ustack_top = USER_TOP - PAGE_SIZE;
    for (usize i = 0; i < USER_STACK_PAGES; ++i) {
        paddr_t pa = pmm_alloc_zero();
        if (!pa) { vmm_destroy_root(root); return SL_ERR_NOMEM; }
        vaddr_t va = ustack_top - i * PAGE_SIZE;
        int rc = vmm_map((u64 *)root, va, pa, PAGE_SIZE,
                         VMM_R | VMM_W | VMM_U);
        if (rc) { vmm_destroy_root(root); return rc; }
    }
    /* Set initial sp to top of stack region, aligned down 16. */
    vaddr_t ustack = ustack_top - 16;

    *out_entry    = (vaddr_t)h->entry;
    *out_root_pa  = root;
    *out_ustack   = ustack;
    kinf("spx: entry=0x%lx root=0x%lx ustack=0x%lx",
         *out_entry, root, *out_ustack);
    return 0;
}
