// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — minimal FDT parser — core.
 */
#include "types.h"
#include "fdt.h"
#include "klog.h"
#include "riscv.h"

#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

const void *g_dtb = NULL;
const u32 *g_struct = NULL;
const char *g_strings = NULL;
u32 g_struct_size = 0;

u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

u64 be64(const u8 *p)
{
    return ((u64)be32(p) << 32) | be32(p + 4);
}

static bool str_starts(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) return false;
    }
    return true;
}

void fdt_init(const void *dtb)
{
    g_dtb = dtb;
    const fdt_header *h = (const fdt_header *)dtb;
    if (be32((const u8 *)&h->magic) != FDT_MAGIC) {
        kerr("FDT: bad magic 0x%x", be32((const u8 *)&h->magic));
        g_dtb = NULL;
        return;
    }
    u32 off_struct = be32((const u8 *)&h->off_dt_struct);
    u32 off_strings = be32((const u8 *)&h->off_dt_strings);
    u32 size_struct = be32((const u8 *)&h->size_dt_struct);
    g_struct = (const u32 *)((const u8 *)dtb + off_struct);
    g_strings = (const char *)((const u8 *)dtb + off_strings);
    g_struct_size = size_struct;
}

const void *fdt_raw(void) { return g_dtb; }

/* --- find /model --- */

struct model_priv { const char *out; };
static int model_cb(const walk_node_t *n, void *priv)
{
    struct model_priv *mp = priv;
    u32 len = 0;
    const u8 *v = find_prop(n, "model", &len);
    if (v && len > 0) { mp->out = (const char *)v; return 1; }
    return 0;
}

const char *fdt_model(const char *fallback)
{
    struct model_priv mp = { NULL };
    walk(model_cb, &mp);
    return mp.out ? mp.out : fallback;
}

/* --- find /memory --- */

struct mem_priv { fdt_memory_t *out; bool found; };
static int mem_cb(const walk_node_t *n, void *priv)
{
    struct mem_priv *mp = priv;
    if (!str_starts(n->name, "memory")) return 0;
    u32 len = 0;
    const u8 *v = find_prop(n, "reg", &len);
    if (!v || len < 16) return 0;
    mp->out->base = be64(v);
    mp->out->size = be64(v + 8);
    mp->found = true;
    return 1;
}

bool fdt_memory(fdt_memory_t *out)
{
    struct mem_priv mp = { out, false };
    walk(mem_cb, &mp);
    return mp.found;
}
