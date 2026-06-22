/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — FAT32 read-only mount (minimal).
 * Stub in MVP; used to read /bin/init from an existing FAT image if SlipperFS
 * is not present.
 */
#ifndef SLIPPER_FAT32_H
#define SLIPPER_FAT32_H

#include "types.h"

int  fat32_mount(int virtio_dev);
int  fat32_lookup(const char *path, u32 *out_cluster, u32 *out_size);
int  fat32_read(u32 cluster, void *buf, u32 off, u32 len);

#endif
