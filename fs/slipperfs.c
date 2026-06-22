// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — SlipperFS runtime.
 *
 * Reads from a SlipperFS image on a virtio-blk device. Read-only in MVP.
 */
#include "types.h"
#include "slipperfs.h"
#include "virtio.h"
#include "heap.h"
#include "klog.h"
#include "riscv.h"

static int    g_dev = -1;
static u64    g_lba_base;   /* SlipperFS partition LBA offset */
static spfs_super_t  g_sb;
static u8     g_buf[SPFS_BLOCK_SIZE] __attribute__((aligned(8)));

static int read_block(u32 blk, void *buf)
{
    /* SPFS_BLOCK_SIZE = 4096 = 8 * 512. */
    u8 *p = (u8 *)buf;
    u64 lba = g_lba_base + (u64)blk * 8;
    for (int i = 0; i < 8; ++i) {
        int rc = virtio_blk_read(g_dev, lba + i, p + i * 512);
        if (rc < 0) return rc;
    }
    return 0;
}

int spfs_mount(int virtio_dev, u64 lba_offset)
{
    g_dev = virtio_dev;
    g_lba_base = lba_offset;
    if (read_block(0, &g_sb) < 0) {
        kerr("slipperfs: cannot read superblock");
        return SL_ERR_IO;
    }
    if (g_sb.magic != SPFS_MAGIC) {
        kerr("slipperfs: bad magic 0x%x", g_sb.magic);
        return SL_ERR_INVAL;
    }
    if (g_sb.block_size != SPFS_BLOCK_SIZE) {
        kerr("slipperfs: block_size=%u unsupported", g_sb.block_size);
        return SL_ERR_INVAL;
    }
    kinf("slipperfs: mounted v%u, %u blocks, %u inodes",
         g_sb.version, g_sb.total_blocks, g_sb.inode_count);
    return 0;
}

static int read_inode(u32 ino, spfs_inode_t *out)
{
    if (ino == 0 || ino > g_sb.inode_count) return SL_ERR_NOENT;
    /* Inode N lives at block (inode_table_start) + (N-1) / 64,
     * slot (N-1) % 64. */
    u32 blk   = g_sb.inode_table_start + (ino - 1) / (SPFS_BLOCK_SIZE / sizeof(spfs_inode_t));
    u32 slot  = (ino - 1) % (SPFS_BLOCK_SIZE / sizeof(spfs_inode_t));
    if (read_block(blk, g_buf) < 0) return SL_ERR_IO;
    spfs_inode_t *table = (spfs_inode_t *)g_buf;
    *out = table[slot];
    return 0;
}

int spfs_stat(u32 ino, spfs_stat_t *st)
{
    spfs_inode_t in;
    int rc = read_inode(ino, &in);
    if (rc) return rc;
    st->ino  = ino;
    st->size = in.size;
    st->mode = in.mode;
    return 0;
}

/* Lookup a name in the root directory catalog.
 * Root inode's data block contains an array of spfs_dirent_t.
 * Each entry: 32 bytes name + 4 bytes inode. 32 / 36 entries per block. */
int spfs_lookup(const char *name, spfs_stat_t *st)
{
    spfs_inode_t root;
    int rc = read_inode(g_sb.root_inode, &root);
    if (rc) return rc;
    if (root.blocks[0] == 0) return SL_ERR_NOENT;
    if (read_block(root.blocks[0], g_buf) < 0) return SL_ERR_IO;
    spfs_dirent_t *ents = (spfs_dirent_t *)g_buf;
    int n = SPFS_BLOCK_SIZE / sizeof(spfs_dirent_t);
    for (int i = 0; i < n; ++i) {
        if (ents[i].inode == 0) continue;
        if (ents[i].name[0] == 0) continue;
        /* Compare up to SPFS_NAME_MAX. */
        bool match = true;
        for (int j = 0; j < SPFS_NAME_MAX; ++j) {
            if (ents[i].name[j] != name[j]) { match = false; break; }
            if (ents[i].name[j] == 0) break;
        }
        if (match) {
            return spfs_stat(ents[i].inode, st);
        }
    }
    return SL_ERR_NOENT;
}

int spfs_read(u32 ino, void *buf, u32 off, u32 len)
{
    spfs_inode_t in;
    int rc = read_inode(ino, &in);
    if (rc) return rc;
    if (off >= in.size) return 0;
    if (off + len > in.size) len = in.size - off;

    u32 copied = 0;
    u8 *out = (u8 *)buf;
    while (copied < len) {
        u32 cur_off = off + copied;
        u32 blk_no  = cur_off / SPFS_BLOCK_SIZE;
        u32 blk_off = cur_off % SPFS_BLOCK_SIZE;
        u32 chunk   = MIN(SPFS_BLOCK_SIZE - blk_off, len - copied);
        if (blk_no >= SPFS_DIRECT_BLKS) {
            /* TODO: indirect blocks. */
            kerr("slipperfs: read past direct blocks (ino=%u)", ino);
            return (int)copied;
        }
        u32 phys = in.blocks[blk_no];
        if (phys == 0) {
            /* hole: zeros */
            for (u32 i = 0; i < chunk; ++i) out[copied + i] = 0;
        } else {
            if (read_block(phys, g_buf) < 0) return (int)copied;
            for (u32 i = 0; i < chunk; ++i) out[copied + i] = g_buf[blk_off + i];
        }
        copied += chunk;
    }
    return (int)copied;
}
