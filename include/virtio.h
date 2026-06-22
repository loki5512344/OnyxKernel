/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — virtio-blk MMIO driver (legacy v1 + modern v2).
 * Polled I/O only in MVP.
 */
#ifndef SLIPPER_VIRTIO_H
#define SLIPPER_VIRTIO_H

#include "types.h"

#define VIRTIO_MAX_DEVS   4
#define VIRTIO_BLK_SECTOR 512

#define VIRTQ_SIZE   256

/* virtio MMIO register offsets (legacy v1) */
#define VIRTIO_MMIO_MAGIC_VALUE     0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID       0x00C
#define VIRTIO_MMIO_HOST_FEATURES   0x010
#define VIRTIO_MMIO_HOST_FEATURES_SEL 0x014
#define VIRTIO_MMIO_GUEST_FEATURES  0x020
#define VIRTIO_MMIO_GUEST_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_ALIGN     0x03C
#define VIRTIO_MMIO_QUEUE_PFN       0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK   0x064
#define VIRTIO_MMIO_STATUS          0x070

/* modern (v2) additional regs */
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW   0x0A0
#define VIRTIO_MMIO_QUEUE_USED_HIGH  0x0A4
#define VIRTIO_MMIO_QUEUE_ENABLE     0x044

#define VIRTIO_STATUS_ACK       0x01
#define VIRTIO_STATUS_DRIVER    0x02
#define VIRTIO_STATUS_DRIVER_OK 0x04
#define VIRTIO_STATUS_FEATURES_OK 0x08

#define VIRTIO_ID_BLK           2

/* virtio-blk request header */
#define VIRTIO_BLK_T_IN         0
#define VIRTIO_BLK_T_OUT        1
#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

typedef struct {
    u32 type;
    u32 reserved;
    u64 sector;
    u8  data[512];
    u8  status;
} __attribute__((packed)) blk_req_t;

/* virtqueue descriptor */
typedef struct {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} vq_desc_t;

#define VQ_DESC_F_NEXT      1
#define VQ_DESC_F_WRITE     2
#define VQ_DESC_F_INDIRECT  4

typedef struct {
    u16 flags;
    u16 idx;
    u16 ring[VIRTQ_SIZE];
    u16 used_event;
} vq_avail_t;

typedef struct {
    u16 flags;
    u16 idx;
    struct {
        u32 idx;
        u32 len;
    } ring[VIRTQ_SIZE];
    u16 avail_event;
} vq_used_t;

struct virtio_blk_dev {
    uptr        base;
    bool        modern;
    u32         version;
    vq_desc_t  *desc;
    vq_avail_t *avail;
    vq_used_t  *used;
    u16         last_used;
    blk_req_t  *req_buf;
};

int  virtio_blk_probe(uptr mmio_base);
int  virtio_blk_init(uptr mmio_base);
int  virtio_blk_read(int dev, u64 lba, void *buf);
int  virtio_blk_write(int dev, u64 lba, const void *buf);
int  virtio_blk_count(void);
uptr virtio_blk_base(int dev);

#endif
