/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — Flattened Device Tree parser (read-only, minimal).
 * Just enough to find: /memory, /model, NS16550A UART, virtio,mmio,
 * sdhci nodes. No overlays, no phandle resolution.
 */
#ifndef SLIPPER_FDT_H
#define SLIPPER_FDT_H

#include "types.h"

#define FDT_MAGIC       0xD00DFEED
#define FDT_MAX_DEPTH   16

typedef struct {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
} fdt_header;

typedef struct {
    paddr_t base;
    usize   size;
} fdt_memory_t;

typedef struct {
    paddr_t base;
    u32     irq;
    u32     reg_shift;       /* for UART: 0,1,2; virtio: irrelevant */
} fdt_mmio_t;

/* Walker types — shared between fdt.c and fdt_find.c */
typedef struct {
    const char *name;       /* node name (without @addr) */
    const char *path;       /* full path */
    const u32  *props;      /* pointer to first FDT_PROP for this node */
    u32         prop_bytes; /* bytes of props */
} walk_node_t;

typedef int (*node_cb_t)(const walk_node_t *node, void *priv);

/* Walk the device tree, calling cb for each node. Returns nodes visited. */
int  walk(node_cb_t cb, void *priv);
/* Find a property by name in the current node. */
const u8 *find_prop(const walk_node_t *n, const char *name, u32 *out_len);

void  fdt_init(const void *dtb);
const char *fdt_model(const char *fallback);
bool  fdt_memory(fdt_memory_t *out);
int   fdt_find_uart(fdt_mmio_t *out);                         /* first NS16550A */
int   fdt_find_virtio(fdt_mmio_t *out, int max);              /* virtio,mmio */
int   fdt_find_sdhci(fdt_mmio_t *out, int max);
int   fdt_find_clint(paddr_t *out);                           /* riscv,clint0 */
int   fdt_find_plic(paddr_t *out);                            /* riscv,plic0 */

/* raw walk helper, for debugging */
const void *fdt_raw(void);

#endif
