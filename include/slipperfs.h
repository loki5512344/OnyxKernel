/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — SlipperFS on-disk format and runtime API.
 *
 * Layout (4K-block filesystem):
 *   [ super | inode_bitmap | data_bitmap | inode_table | data blocks ]
 *
 * Superblock is block 0. Inode bitmap is block 1. Data bitmap is block 2.
 * Inode table starts at block 3, occupies inode_count / 8 blocks.
 *
 * This is intentionally simple — no journaling, no symlinks, no xattrs.
 */
#ifndef SLIPPER_SLIPPERFS_H
#define SLIPPER_SLIPPERFS_H

#include "types.h"

#define SPFS_MAGIC        0x31504C53   /* 'SLP1' little-endian */
#define SPFS_BLOCK_SIZE   4096
#define SPFS_NAME_MAX     32
#define SPFS_DIRECT_BLKS  10
#define SPFS_ROOT_INO     1

typedef struct {
    u32 magic;
    u32 version;            /* = 1 */
    u32 block_size;         /* = 4096 */
    u32 total_blocks;
    u32 inode_count;
    u32 inode_table_start;  /* block index */
    u32 data_bitmap_start;  /* block index */
    u32 data_blocks_start;  /* block index */
    u32 root_inode;         /* = 1 */
    u32 reserved[7];
} spfs_super_t;

_Static_assert(sizeof(spfs_super_t) <= SPFS_BLOCK_SIZE, "superblock fits one block");

typedef struct {
    u32 mode;               /* file type + permissions (S_IFREG=0100644 etc.) */
    u32 size;               /* bytes */
    u32 blocks[SPFS_DIRECT_BLKS];   /* block indices into data_blocks region */
    u32 indirect;           /* single-indirect block (0 if none) */
    u32 reserved[3];
} spfs_inode_t;

_Static_assert(sizeof(spfs_inode_t) == 64, "inode is 64 bytes");

/* Catalog entry stored at start of root inode data. */
typedef struct {
    char name[SPFS_NAME_MAX];
    u32  inode;
} spfs_dirent_t;

#define SPFS_DT_REG  1
#define SPFS_DT_DIR  2

typedef struct {
    u32 ino;
    u32 size;
    u32 mode;
} spfs_stat_t;

int  spfs_mount(int virtio_dev, u64 lba_offset);
int  spfs_lookup(const char *name, spfs_stat_t *st);   /* root-relative, flat */
int  spfs_read(u32 ino, void *buf, u32 off, u32 len);
int  spfs_stat(u32 ino, spfs_stat_t *st);

/* Used by mkimage.sh / mkimage.py to build the image from host. */
/* No host code here, see scripts/mkimage.py. */

#endif
