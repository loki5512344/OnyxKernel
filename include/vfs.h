/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — minimal VFS layer.
 */
#ifndef SLIPPER_VFS_H
#define SLIPPER_VFS_H

#include "types.h"

#define VFS_MAX_FDS  16

typedef enum {
    VFS_FS_NONE = 0,
    VFS_FS_SLIPPER,
    VFS_FS_FAT32,
} vfs_fs_t;

typedef struct {
    u32   ino;          /* SlipperFS inode or FAT32 first cluster */
    u32   size;
    u32   pos;
    u8    fs;           /* vfs_fs_t */
    u8    used;
} vfs_fd_t;

int  vfs_init(void);
int  vfs_mount_root(int virtio_dev, u64 slipperfs_lba);  /* tries SlipperFS, then FAT32 */
int  vfs_open(const char *path);      /* returns fd >= 0, < 0 on error */
int  vfs_read(int fd, void *buf, usize len);
int  vfs_close(int fd);
int  vfs_stat(int fd, u32 *size_out);

vfs_fd_t *vfs_get_fd(int fd);

#endif
