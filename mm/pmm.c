// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — physical memory manager (4K bitmap).
 *
 * PMM owns DRAM above the kernel image. We do NOT manage the bootloader
 * region [0x80000000..kernel_end) — it stays reserved.
 */
#include "types.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "klog.h"
#include "riscv.h"

static u8    *g_bitmap = NULL;
static usize  g_bitmap_pages = 0;   /* total 4K pages managed */
static usize  g_free_pages = 0;
static paddr_t g_base = 0;          /* first managed PA */

void pmm_init(paddr_t dram_base, usize dram_size)
{
    /* Reserve low part: bootloader + kernel + heap region (4MB above kernel). */
    paddr_t kernel_end = (paddr_t)&__kernel_end;
    paddr_t heap_end   = kernel_end + MB(4);
    paddr_t managed_base = heap_end;

    /* align up to 4K */
    managed_base = (managed_base + PAGE_MASK) & ~PAGE_MASK;
    if (managed_base < dram_base) managed_base = dram_base;
    paddr_t dram_top = dram_base + dram_size;
    if (managed_base >= dram_top) {
        kpanic("PMM: no memory after kernel");
    }
    usize managed_bytes = dram_top - managed_base;
    usize pages = managed_bytes / PAGE_SIZE;

    /* Place bitmap at the very start of managed region. */
    usize bitmap_bytes = (pages + 7) / 8;
    usize bitmap_pages = (bitmap_bytes + PAGE_MASK) / PAGE_SIZE;
    g_bitmap = (u8 *)managed_base;     /* identity-mapped, writeable */
    /* zero bitmap */
    for (usize i = 0; i < bitmap_bytes; ++i) g_bitmap[i] = 0;
    /* mark bitmap pages as used */
    for (usize i = 0; i < bitmap_pages; ++i) {
        g_bitmap[i / 8] |= (1 << (i % 8));
    }

    g_base = managed_base + bitmap_pages * PAGE_SIZE;
    g_bitmap_pages = pages - bitmap_pages;
    g_free_pages = g_bitmap_pages;

    kinf("PMM: dram 0x%lx + 0x%lx", dram_base, dram_size);
    kinf("PMM: managed 0x%lx, pages=%lu free=%lu",
         g_base, g_bitmap_pages, g_free_pages);
}

static bool bm_get(usize i)
{
    return (g_bitmap[i / 8] >> (i % 8)) & 1;
}
static void bm_set(usize i)
{
    if (!(g_bitmap[i / 8] & (1 << (i % 8)))) {
        g_bitmap[i / 8] |= (1 << (i % 8));
        g_free_pages--;
    }
}
static void bm_clr(usize i)
{
    if (g_bitmap[i / 8] & (1 << (i % 8))) {
        g_bitmap[i / 8] &= ~(1 << (i % 8));
        g_free_pages++;
    }
}

paddr_t pmm_alloc(void)
{
    for (usize i = 0; i < g_bitmap_pages; ++i) {
        if (!bm_get(i)) {
            bm_set(i);
            return g_base + i * PAGE_SIZE;
        }
    }
    return 0;
}

paddr_t pmm_alloc_n(usize pages)
{
    if (pages == 0) return 0;
    usize run = 0;
    for (usize i = 0; i < g_bitmap_pages; ++i) {
        if (!bm_get(i)) {
            run++;
            if (run == pages) {
                usize start = i - pages + 1;
                for (usize j = start; j <= i; ++j) bm_set(j);
                return g_base + start * PAGE_SIZE;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

void pmm_free(paddr_t pa)
{
    if (pa < g_base) return;
    usize i = (pa - g_base) / PAGE_SIZE;
    if (i >= g_bitmap_pages) return;
    bm_clr(i);
}

void pmm_free_n(paddr_t pa, usize pages)
{
    for (usize i = 0; i < pages; ++i) pmm_free(pa + i * PAGE_SIZE);
}

void pmm_reserve(paddr_t pa, usize pages)
{
    /* pa may be inside the bootloader/kernel region; just mark as used. */
    for (usize i = 0; i < pages; ++i) {
        paddr_t p = pa + i * PAGE_SIZE;
        if (p < g_base) continue;
        usize idx = (p - g_base) / PAGE_SIZE;
        if (idx >= g_bitmap_pages) continue;
        bm_set(idx);
    }
}

paddr_t pmm_alloc_zero(void)
{
    paddr_t pa = pmm_alloc();
    if (!pa) return 0;
    u64 *p = (u64 *)pa;
    for (usize i = 0; i < PAGE_SIZE / 8; ++i) p[i] = 0;
    return pa;
}

usize pmm_free_pages(void) { return g_free_pages; }
usize pmm_total_pages(void) { return g_bitmap_pages; }
