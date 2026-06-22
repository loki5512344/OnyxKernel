#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SPFS_MAGIC        0x31504C53
#define SPFS_VERSION      1
#define SPFS_BLOCK_SIZE   4096
#define SPFS_NAME_MAX     32
#define SPFS_DIRECT_BLKS  10
#define SPFS_ROOT_INO     1

#define BLOCK_SUPER        0
#define BLOCK_INODE_BITMAP 1
#define BLOCK_DATA_BITMAP  2
#define BLOCK_INODE_TABLE  3
#define BLOCK_DATA_START   4

#define DIRENTS_PER_BLOCK (SPFS_BLOCK_SIZE / (SPFS_NAME_MAX + 4))

struct spfs_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t inode_table_start;
    uint32_t data_bitmap_start;
    uint32_t data_blocks_start;
    uint32_t root_inode;
    uint32_t reserved[7];
} __attribute__((packed));

struct spfs_inode {
    uint32_t mode;
    uint32_t size;
    uint32_t blocks[SPFS_DIRECT_BLKS];
    uint32_t indirect;
    uint32_t reserved[3];
} __attribute__((packed));

struct spfs_dirent {
    char name[SPFS_NAME_MAX];
    uint32_t inode;
} __attribute__((packed));

_Static_assert(sizeof(struct spfs_superblock) == 64, "superblock must be 64 bytes");
_Static_assert(sizeof(struct spfs_inode) == 64, "inode must be 64 bytes");
_Static_assert(sizeof(struct spfs_dirent) == 36, "dirent must be 36 bytes");

static void die(const char *msg)
{
    fprintf(stderr, "mkimage: error: %s\n", msg);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc != 3)
        die("usage: mkimage init.spx disk.img");

    FILE *fin = fopen(argv[1], "rb");
    if (!fin) die("cannot open init.spx");
    fseek(fin, 0, SEEK_END);
    long init_size = ftell(fin);
    rewind(fin);
    unsigned char *init_data = malloc(init_size > 0 ? (size_t)init_size : 1);
    if (!init_data) die("malloc");
    if (init_size > 0 && fread(init_data, 1, init_size, fin) != (size_t)init_size)
        die("read");
    fclose(fin);

    int init_blocks = (init_size + SPFS_BLOCK_SIZE - 1) / SPFS_BLOCK_SIZE;
    int total_blocks = BLOCK_DATA_START + 1 + init_blocks;

    unsigned char *blocks = calloc(total_blocks, SPFS_BLOCK_SIZE);
    if (!blocks) die("calloc");

    /* 1) Superblock */
    struct spfs_superblock sb;
    sb.magic             = SPFS_MAGIC;
    sb.version           = SPFS_VERSION;
    sb.block_size        = SPFS_BLOCK_SIZE;
    sb.total_blocks      = total_blocks;
    sb.inode_count       = 32;
    sb.inode_table_start = BLOCK_INODE_TABLE;
    sb.data_bitmap_start = BLOCK_DATA_BITMAP;
    sb.data_blocks_start = BLOCK_DATA_START;
    sb.root_inode        = SPFS_ROOT_INO;
    memset(sb.reserved, 0, sizeof(sb.reserved));
    memcpy(&blocks[BLOCK_SUPER * SPFS_BLOCK_SIZE], &sb, sizeof(sb));

    /* 2) Inode bitmap: ino 1 (root) + ino 2 (init) used => bits 0 and 1 */
    blocks[BLOCK_INODE_BITMAP * SPFS_BLOCK_SIZE] = 0x03;

    /* 3) Data bitmap: data block 0 (dirents), data block 1..N (init content) */
    {
        int nbits = 1 + init_blocks;
        for (int i = 0; i < nbits; i++) {
            int byte_off = BLOCK_DATA_BITMAP * SPFS_BLOCK_SIZE + i / 8;
            blocks[byte_off] |= (unsigned char)(1 << (i % 8));
        }
    }

    /* 4) Inode table: ino 1 = root dir, ino 2 = /bin/init */
    struct spfs_inode root_ino;
    memset(&root_ino, 0, sizeof(root_ino));
    root_ino.mode  = 0100755;
    root_ino.size  = DIRENTS_PER_BLOCK * (SPFS_NAME_MAX + 4);
    root_ino.blocks[0] = BLOCK_DATA_START;
    root_ino.indirect   = 0;
    memcpy(&blocks[BLOCK_INODE_TABLE * SPFS_BLOCK_SIZE], &root_ino, sizeof(root_ino));

    struct spfs_inode init_ino;
    memset(&init_ino, 0, sizeof(init_ino));
    init_ino.mode = 0100755;
    init_ino.size = init_size;
    for (int i = 0; i < init_blocks && i < SPFS_DIRECT_BLKS; i++)
        init_ino.blocks[i] = BLOCK_DATA_START + 1 + i;
    init_ino.indirect = 0;
    memcpy(&blocks[BLOCK_INODE_TABLE * SPFS_BLOCK_SIZE + 64],
           &init_ino, sizeof(init_ino));

    /* 5) Data block 0: root directory entries (".", "bin/init") */
    {
        unsigned char *db = &blocks[BLOCK_DATA_START * SPFS_BLOCK_SIZE];
        struct spfs_dirent *de;

        de = (struct spfs_dirent *)db;
        memset(de->name, 0, SPFS_NAME_MAX);
        de->name[0] = '.';
        de->inode = 1;

        de = (struct spfs_dirent *)(db + sizeof(struct spfs_dirent));
        memset(de->name, 0, SPFS_NAME_MAX);
        memcpy(de->name, "bin/init", 8);
        de->inode = 2;
    }

    /* 6) Data blocks 1..N: contents of init.spx */
    for (int i = 0; i < init_blocks; i++) {
        size_t offset = (size_t)i * SPFS_BLOCK_SIZE;
        size_t chunk = SPFS_BLOCK_SIZE;
        if (offset + chunk > (size_t)init_size)
            chunk = init_size - offset;
        memcpy(&blocks[(BLOCK_DATA_START + 1 + i) * SPFS_BLOCK_SIZE],
               init_data + offset, chunk);
    }

    /* Pad to 512-byte boundary for virtio-blk */
    size_t img_size = (size_t)total_blocks * SPFS_BLOCK_SIZE;
    size_t padded   = (img_size + 511) & ~(size_t)511;
    unsigned char *out = malloc(padded);
    if (!out) die("malloc");
    memcpy(out, blocks, img_size);
    if (padded > img_size)
        memset(out + img_size, 0, padded - img_size);

    FILE *fout = fopen(argv[2], "wb");
    if (!fout) die("cannot open output");
    if (fwrite(out, 1, padded, fout) != padded)
        die("write failed");
    fclose(fout);

    free(out);
    free(blocks);
    free(init_data);

    printf("mkimage: wrote %s (%zu bytes, %d blocks, init=%ldB/%d blocks)\n",
           argv[2], padded, total_blocks, init_size, init_blocks);
    return 0;
}
