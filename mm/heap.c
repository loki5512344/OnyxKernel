// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — kernel heap.
 *
 * Bump allocator over a 4MB region right after the kernel image, with
 * a free-list for power-of-two sizes >= 32 bytes. Page allocations go
 * through PMM.
 */
#include "types.h"
#include "heap.h"
#include "vmm.h"
#include "pmm.h"
#include "klog.h"
#include "riscv.h"

#define HEAP_SIZE  MB(4)
#define ALIGN8(x)  (((usize)(x) + 7) & ~7UL)

static u8    *g_heap_base = NULL;
static usize  g_heap_used = 0;
static usize  g_heap_size = 0;

typedef struct block {
    usize         size;
    bool          free;
    struct block *next;
    struct block *prev;
} block_t;

static block_t *g_free_head = NULL;

void heap_init(void)
{
    paddr_t kend = (paddr_t)&__kernel_end;
    g_heap_base = (u8 *)kend;
    g_heap_size = HEAP_SIZE;
    g_heap_used = 0;
    g_free_head = NULL;
    kinf("heap: 0x%lx + 0x%lx", (usize)g_heap_base, g_heap_size);
}

void *kmalloc(usize size)
{
    if (size == 0) return NULL;
    size = ALIGN8(size) + sizeof(block_t);

    /* Try free list first. */
    for (block_t *b = g_free_head; b; b = b->next) {
        if (b->free && b->size >= size) {
            /* Split if there's room for another header + 16 bytes. */
            if (b->size >= size + sizeof(block_t) + 16) {
                block_t *split = (block_t *)((u8 *)b + size);
                split->size = b->size - size;
                split->free = true;
                split->next = b->next;
                split->prev = b;
                if (b->next) b->next->prev = split;
                b->next = split;
                b->size = size;
            }
            b->free = false;
            return (u8 *)b + sizeof(block_t);
        }
    }

    /* Bump. */
    if (g_heap_used + size > g_heap_size) {
        kerr("heap: OOM (used=%lu want=%lu cap=%lu)", g_heap_used, size, g_heap_size);
        return NULL;
    }
    block_t *b = (block_t *)(g_heap_base + g_heap_used);
    b->size = size;
    b->free = false;
    b->next = g_free_head;
    b->prev = NULL;
    if (g_free_head) g_free_head->prev = b;
    g_free_head = b;
    g_heap_used += size;
    return (u8 *)b + sizeof(block_t);
}

void *kmalloc_aligned(usize size, usize align)
{
    if (align <= 8) return kmalloc(size);
    /* Worst case: allocate size+align+header. */
    usize extra = align + sizeof(block_t);
    void *p = kmalloc(size + extra);
    if (!p) return NULL;
    usize addr = (usize)p + sizeof(block_t);
    usize aligned = (addr + align - 1) & ~(align - 1);
    /* Not optimal — wastes the prefix — but correct. */
    return (void *)aligned;
}

void kfree(void *p)
{
    if (!p) return;
    block_t *b = (block_t *)((u8 *)p - sizeof(block_t));
    b->free = true;
    /* TODO: coalesce with neighbours. */
}

void *krealloc(void *p, usize new_size)
{
    if (!p) return kmalloc(new_size);
    if (new_size == 0) { kfree(p); return NULL; }
    block_t *b = (block_t *)((u8 *)p - sizeof(block_t));
    usize avail = b->size - sizeof(block_t);
    if (avail >= new_size) return p;
    void *np = kmalloc(new_size);
    if (!np) return NULL;
    /* copy */
    u8 *s = (u8 *)p;
    u8 *d = (u8 *)np;
    for (usize i = 0; i < avail && i < new_size; ++i) d[i] = s[i];
    kfree(p);
    return np;
}

paddr_t heap_alloc_page(void)
{
    return pmm_alloc_zero();
}

void heap_free_page(paddr_t pa)
{
    pmm_free(pa);
}

usize heap_used(void) { return g_heap_used; }
usize heap_free(void) { return g_heap_size - g_heap_used; }
