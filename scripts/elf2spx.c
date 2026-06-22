#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SPX_MAGIC    0x31585053
#define SPX_VERSION  1
#define SPX_MAX_SEGS 8

#define PT_LOAD 1
#define PF_X    1
#define PF_W    2
#define PF_R    4

#define VMM_R (1 << 1)
#define VMM_W (1 << 2)
#define VMM_X (1 << 3)

#define EM_RISCV 243
#define ET_EXEC  2

struct spx_segment {
    uint64_t vaddr;
    uint64_t filesz;
    uint64_t memsz;
    uint32_t offset;
    uint32_t flags;
    uint32_t align;
    uint32_t reserved;
} __attribute__((packed));

struct spx_header {
    uint32_t magic;
    uint32_t version;
    uint64_t entry;
    uint32_t nsegs;
    uint32_t flags;
    struct spx_segment segs[SPX_MAX_SEGS];
} __attribute__((packed));

_Static_assert(sizeof(struct spx_header) == 344, "spx_header must be 344 bytes");
_Static_assert(sizeof(struct spx_segment) == 40, "spx_segment must be 40 bytes");

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

static void die(const char *msg)
{
    fprintf(stderr, "elf2spx: error: %s\n", msg);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc != 3)
        die("usage: elf2spx input.elf output.spx");

    FILE *fin = fopen(argv[1], "rb");
    if (!fin) die("cannot open input file");
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    if (fsize < (long)sizeof(Elf64_Ehdr))
        die("file too small");
    rewind(fin);
    unsigned char *elf = malloc(fsize);
    if (!elf) die("malloc failed");
    if (fread(elf, 1, fsize, fin) != (size_t)fsize)
        die("read failed");
    fclose(fin);

    if (memcmp(elf, "\x7f""ELF", 4) != 0)
        die("not an ELF file");
    if (elf[4] != 2)
        die("only ELF64 is supported");
    if (elf[5] != 1)
        die("only little-endian ELF is supported");

    Elf64_Ehdr *eh = (Elf64_Ehdr *)elf;
    if (eh->e_type != ET_EXEC)
        die("not an executable (ET_EXEC)");
    if (eh->e_machine != EM_RISCV)
        die("not a RISC-V ELF");
    if (eh->e_phentsize != sizeof(Elf64_Phdr))
        die("unexpected program header size");

    int phnum = eh->e_phnum;
    uint64_t phoff = eh->e_phoff;
    uint64_t ph_end = phoff + (uint64_t)phnum * eh->e_phentsize;
    if (ph_end > (uint64_t)fsize)
        die("truncated program headers");

    int nsegs = 0;
    for (int i = 0; i < phnum; i++) {
        Elf64_Phdr *ph = (Elf64_Phdr *)(elf + phoff + i * eh->e_phentsize);
        if (ph->p_type == PT_LOAD)
            nsegs++;
    }
    if (nsegs == 0)
        die("no PT_LOAD segments");
    if (nsegs > SPX_MAX_SEGS)
        die("too many PT_LOAD segments");

    uint32_t data_off = sizeof(struct spx_header);
    struct spx_header hdr;
    hdr.magic = SPX_MAGIC;
    hdr.version = SPX_VERSION;
    hdr.entry = eh->e_entry;
    hdr.nsegs = nsegs;
    hdr.flags = 0;
    memset(hdr.segs, 0, sizeof(hdr.segs));

    uint64_t total_data = 0;
    int seg_idx = 0;
    for (int i = 0; i < phnum && seg_idx < nsegs; i++) {
        Elf64_Phdr *ph = (Elf64_Phdr *)(elf + phoff + i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD)
            continue;

        uint32_t flags = 0;
        if (ph->p_flags & PF_R) flags |= VMM_R;
        if (ph->p_flags & PF_W) flags |= VMM_W;
        if (ph->p_flags & PF_X) flags |= VMM_X;

        hdr.segs[seg_idx].vaddr    = ph->p_vaddr;
        hdr.segs[seg_idx].filesz   = ph->p_filesz;
        hdr.segs[seg_idx].memsz    = ph->p_memsz;
        hdr.segs[seg_idx].offset   = data_off + total_data;
        hdr.segs[seg_idx].flags    = flags;
        hdr.segs[seg_idx].align    = ph->p_align;
        hdr.segs[seg_idx].reserved = 0;

        if (ph->p_offset + ph->p_filesz > (uint64_t)fsize)
            die("segment data truncated");
        total_data += ph->p_filesz;
        seg_idx++;
    }

    FILE *fout = fopen(argv[2], "wb");
    if (!fout)
        die("cannot open output file");
    if (fwrite(&hdr, sizeof(hdr), 1, fout) != 1)
        die("write header failed");

    for (int i = 0; i < phnum; i++) {
        Elf64_Phdr *ph = (Elf64_Phdr *)(elf + phoff + i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD)
            continue;
        if (fwrite(elf + ph->p_offset, 1, ph->p_filesz, fout) != ph->p_filesz)
            die("write segment data failed");
    }

    fclose(fout);

    uint64_t entry = eh->e_entry;
    free(elf);

    printf("elf2spx: wrote %s (%zu bytes, %d segments, entry=0x%llx)\n",
           argv[2], (size_t)(sizeof(hdr) + total_data), nsegs,
           (unsigned long long)entry);
    return 0;
}
