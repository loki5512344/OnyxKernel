// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — virtio-blk MMIO driver — core & init.
 * Polled I/O only.
 *
 * Adapted from SlipperBoot/include/virtio.hpp, ported to C and adapted
 * for the kernel (heap-allocated VirtQueue, dev table).
 */
#include "types.h"
#include "virtio.h"
#include "heap.h"
#include "pmm.h"
#include "klog.h"
#include "riscv.h"

struct virtio_blk_dev g_devs[VIRTIO_MAX_DEVS];
int g_ndevs = 0;

static inline void reg_w(uptr base, u32 off, u32 v)
{
    *(volatile u32 *)(base + off) = v;
}
static inline u32 reg_r(uptr base, u32 off)
{
    return *(volatile u32 *)(base + off);
}

static u32 v2_read_features(uptr base)
{
    reg_w(base, VIRTIO_MMIO_HOST_FEATURES_SEL, 0);
    return reg_r(base, VIRTIO_MMIO_HOST_FEATURES);
}
static void v2_write_features(uptr base, u32 v)
{
    reg_w(base, VIRTIO_MMIO_GUEST_FEATURES_SEL, 0);
    reg_w(base, VIRTIO_MMIO_GUEST_FEATURES, v);
}

int virtio_blk_probe(uptr mmio_base)
{
    u32 magic = reg_r(mmio_base, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != 0x74726976) return 0;
    u32 devid = reg_r(mmio_base, VIRTIO_MMIO_DEVICE_ID);
    return (devid == VIRTIO_ID_BLK);
}

static void setup_queue(struct virtio_blk_dev *d)
{
    paddr_t desc_pa = pmm_alloc_zero();
    paddr_t avail_pa = pmm_alloc_zero();
    paddr_t used_pa = pmm_alloc_zero();
    paddr_t req_pa = pmm_alloc_zero();
    if (!desc_pa || !avail_pa || !used_pa || !req_pa) {
        kerr("virtio: OOM allocating queue");
        return;
    }
    d->desc  = (vq_desc_t *)desc_pa;
    d->avail = (vq_avail_t *)avail_pa;
    d->used  = (vq_used_t *)used_pa;
    d->req_buf = (blk_req_t *)req_pa;

    if (d->modern) {
        reg_w(d->base, VIRTIO_MMIO_QUEUE_SEL, 0);
        reg_w(d->base, VIRTIO_MMIO_QUEUE_NUM, VIRTQ_SIZE);
        reg_w(d->base, VIRTIO_MMIO_QUEUE_DESC_LOW,  (u32)(desc_pa & 0xFFFFFFFF));
        reg_w(d->base, VIRTIO_MMIO_QUEUE_DESC_HIGH, (u32)(desc_pa >> 32));
        reg_w(d->base, VIRTIO_MMIO_QUEUE_AVAIL_LOW,  (u32)(avail_pa & 0xFFFFFFFF));
        reg_w(d->base, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (u32)(avail_pa >> 32));
        reg_w(d->base, VIRTIO_MMIO_QUEUE_USED_LOW,   (u32)(used_pa & 0xFFFFFFFF));
        reg_w(d->base, VIRTIO_MMIO_QUEUE_USED_HIGH,  (u32)(used_pa >> 32));
        reg_w(d->base, VIRTIO_MMIO_QUEUE_ENABLE, 1);
    } else {
        reg_w(d->base, VIRTIO_MMIO_GUEST_PAGE_SIZE, PAGE_SIZE);
        reg_w(d->base, VIRTIO_MMIO_QUEUE_SEL, 0);
        u32 max = reg_r(d->base, VIRTIO_MMIO_QUEUE_NUM_MAX);
        if (max < VIRTQ_SIZE) {
            kerr("virtio: queue max=%u too small", max);
            return;
        }
        reg_w(d->base, VIRTIO_MMIO_QUEUE_NUM, VIRTQ_SIZE);
        reg_w(d->base, VIRTIO_MMIO_QUEUE_ALIGN, PAGE_SIZE);
        reg_w(d->base, VIRTIO_MMIO_QUEUE_PFN, (u32)(desc_pa >> PAGE_SHIFT));

        pmm_free(desc_pa);
        pmm_free(avail_pa);
        pmm_free(used_pa);
        pmm_free(req_pa);
        desc_pa = pmm_alloc_n(3);
        req_pa  = pmm_alloc_zero();
        if (!desc_pa || !req_pa) {
            kerr("virtio: OOM in legacy queue alloc");
            return;
        }
        d->desc  = (vq_desc_t *)desc_pa;
        d->avail = (vq_avail_t *)(desc_pa + PAGE_SIZE);
        d->used  = (vq_used_t  *)(desc_pa + 2 * PAGE_SIZE);
        d->req_buf = (blk_req_t *)req_pa;
        u8 *p = (u8 *)desc_pa;
        for (usize i = 0; i < 3 * PAGE_SIZE; ++i) p[i] = 0;
        p = (u8 *)req_pa;
        for (usize i = 0; i < PAGE_SIZE; ++i) p[i] = 0;

        reg_w(d->base, VIRTIO_MMIO_QUEUE_PFN, (u32)(desc_pa >> PAGE_SHIFT));
    }

    d->last_used = 0;
}

int virtio_blk_init(uptr mmio_base)
{
    if (g_ndevs >= VIRTIO_MAX_DEVS) return SL_ERR_BUSY;
    if (!virtio_blk_probe(mmio_base)) return SL_ERR_INVAL;

    struct virtio_blk_dev *d = &g_devs[g_ndevs];
    d->base = mmio_base;
    d->version = reg_r(mmio_base, VIRTIO_MMIO_VERSION);
    d->modern = (d->version >= 2);

    reg_w(mmio_base, VIRTIO_MMIO_STATUS, 0);
    reg_w(mmio_base, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    u32 host = d->modern ? v2_read_features(mmio_base)
                         : reg_r(mmio_base, VIRTIO_MMIO_HOST_FEATURES);
    u32 guest = host & 0x1FFFFFFFU;
    if (d->modern) v2_write_features(mmio_base, guest);
    else           reg_w(mmio_base, VIRTIO_MMIO_GUEST_FEATURES, guest);

    if (d->modern) {
        reg_w(mmio_base, VIRTIO_MMIO_STATUS,
              reg_r(mmio_base, VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);
        if (!(reg_r(mmio_base, VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
            kerr("virtio: features not OK");
            return SL_ERR_INVAL;
        }
    }

    setup_queue(d);

    reg_w(mmio_base, VIRTIO_MMIO_STATUS,
          reg_r(mmio_base, VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_DRIVER_OK);

    kinf("virtio-blk[%d] @0x%lx v%u (%s)", g_ndevs, mmio_base,
         d->version, d->modern ? "modern" : "legacy");

    return g_ndevs++;
}

int virtio_blk_count(void) { return g_ndevs; }
uptr virtio_blk_base(int dev) { return (dev >= 0 && dev < g_ndevs) ? g_devs[dev].base : 0; }
