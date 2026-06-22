// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — FAT32 read-only mount.
 * Stub. Real implementation deferred to v0.2.
 */
#include "types.h"
#include "fat32.h"
#include "virtio.h"
#include "klog.h"

int fat32_mount(int virtio_dev)
{
    (void)virtio_dev;
    kwrn("fat32: not implemented yet");
    return SL_ERR_NOSYS;
}

int fat32_lookup(const char *path, u32 *out_cluster, u32 *out_size)
{
    (void)path; (void)out_cluster; (void)out_size;
    return SL_ERR_NOSYS;
}

int fat32_read(u32 cluster, void *buf, u32 off, u32 len)
{
    (void)cluster; (void)buf; (void)off; (void)len;
    return SL_ERR_NOSYS;
}
