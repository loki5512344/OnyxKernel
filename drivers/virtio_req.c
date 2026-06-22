// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — virtio-blk I/O (submit, read, write).
 * Polled I/O only.
 */
#include "types.h"
#include "virtio.h"
#include "klog.h"

extern struct virtio_blk_dev g_devs[VIRTIO_MAX_DEVS];
extern int g_ndevs;

static inline void reg_w(uptr base, u32 off, u32 v)
{
    *(volatile u32 *)(base + off) = v;
}
static inline u32 reg_r(uptr base, u32 off)
{
    return *(volatile u32 *)(base + off);
}

static void submit_and_wait(struct virtio_blk_dev *d, u32 type, u64 sector)
{
    blk_req_t *req = d->req_buf;
    req->type = type;
    req->sector = sector;
    req->status = 0xFF;

    paddr_t req_pa = (paddr_t)req;

    d->desc[0].addr  = req_pa;
    d->desc[0].len   = 16;
    d->desc[0].flags = VQ_DESC_F_NEXT;
    d->desc[0].next  = 1;

    d->desc[1].addr  = req_pa + 16;
    d->desc[1].len   = 512;
    d->desc[1].flags = VQ_DESC_F_NEXT | (type == VIRTIO_BLK_T_IN ? VQ_DESC_F_WRITE : 0);
    d->desc[1].next  = 2;

    d->desc[2].addr  = req_pa + 16 + 512;
    d->desc[2].len   = 1;
    d->desc[2].flags = VQ_DESC_F_WRITE;
    d->desc[2].next  = 0;

    u16 idx = d->avail->idx;
    d->avail->ring[idx % VIRTQ_SIZE] = 0;
    __sync_synchronize();
    d->avail->idx = idx + 1;

    reg_w(d->base, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    while (d->used->idx == d->last_used) {
        asm volatile("" ::: "memory");
    }
    d->last_used = d->used->idx;
}

int virtio_blk_read(int dev, u64 lba, void *buf)
{
    if (dev < 0 || dev >= g_ndevs) return SL_ERR_INVAL;
    struct virtio_blk_dev *d = &g_devs[dev];
    submit_and_wait(d, VIRTIO_BLK_T_IN, lba);
    if (d->req_buf->status != VIRTIO_BLK_S_OK) return SL_ERR_IO;
    u8 *s = d->req_buf->data;
    u8 *p = (u8 *)buf;
    for (int i = 0; i < 512; ++i) p[i] = s[i];
    return 512;
}

int virtio_blk_write(int dev, u64 lba, const void *buf)
{
    if (dev < 0 || dev >= g_ndevs) return SL_ERR_INVAL;
    struct virtio_blk_dev *d = &g_devs[dev];
    const u8 *s = (const u8 *)buf;
    u8 *p = d->req_buf->data;
    for (int i = 0; i < 512; ++i) p[i] = s[i];
    submit_and_wait(d, VIRTIO_BLK_T_OUT, lba);
    if (d->req_buf->status != VIRTIO_BLK_S_OK) return SL_ERR_IO;
    return 512;
}
