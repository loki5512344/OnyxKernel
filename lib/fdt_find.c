// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — FDT device finders.
 */
#include "types.h"
#include "fdt.h"
#include "riscv.h"
extern const u32 *g_struct;
extern const char *g_strings;
extern u32 g_struct_size;
extern u32 be32(const u8 *p);
extern u64 be64(const u8 *p);
static bool str_eq(const char *a, const char *b) { return __builtin_strcmp(a, b) == 0; }
static usize str_len(const char *s) { return __builtin_strlen(s); }
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009
static const char *prop_str(const char *strings, u32 off) { return strings + off; }

const u8 *find_prop(const walk_node_t *n, const char *name, u32 *out_len)
{
    const u32 *p = n->props;
    const u32 *end = (const u32 *)((const u8 *)n->props + n->prop_bytes);
    while (p < end) {
        u32 tok = be32((const u8 *)p++);
        if (tok != FDT_PROP) continue;
        u32 plen = be32((const u8 *)p); p++;
        u32 noff = be32((const u8 *)p); p++;
        const char *pname = prop_str(g_strings, noff);
        if (str_eq(pname, name)) {
            if (out_len) *out_len = plen;
            return (const u8 *)p;
        }
        usize adv = (plen + 3) & ~3UL;
        p = (const u32 *)((const u8 *)p + adv);
    }
    return NULL;
}

int walk(node_cb_t cb, void *priv)
{
    if (!g_struct) return 0;
    const u32 *p = g_struct;
    const u32 *end = (const u32 *)((const u8 *)g_struct + g_struct_size);
    char path[256];
    int depth = 0;
    path[0] = '/'; path[1] = 0;
    while (p < end) {
        u32 tok = be32((const u8 *)p++);
        if (tok == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            usize nl = 0;
            while (name[nl]) nl++;
            nl = (nl + 4) & ~3UL;
            p = (const u32 *)((const u8 *)p + nl);
            char child[256];
            const char *nm = name;
            if (depth > 0) {
                usize i = 0;
                while (path[i] && i < sizeof(child) - 1) { child[i] = path[i]; i++; }
                if (i > 1 && child[i-1] != '/') child[i++] = '/';
                int j = 0;
                while (nm[j] && i < sizeof(child) - 1) child[i++] = nm[j++];
                child[i] = 0;
            } else {
                child[0] = '/'; child[1] = 0;
            }
            int k = 0;
            while (child[k] && k < (int)sizeof(path) - 1) { path[k] = child[k]; k++; }
            path[k] = 0;
            walk_node_t n;
            n.name = (depth == 0) ? "" : nm;
            n.path = path;
            n.props = p;
            n.prop_bytes = 0;
            const u32 *q = p;
            u32 pb = 0;
            while (q < end) {
                u32 t = be32((const u8 *)q);
                if (t == FDT_PROP) {
                    u32 plen = be32((const u8 *)(q + 1));
                    q += 3; pb += 12;
                    usize adv = (plen + 3) & ~3UL;
                    q = (const u32 *)((const u8 *)q + adv);
                    pb += adv;
                } else if (t == FDT_NOP) {
                    q += 1;
                } else break;
            }
            n.prop_bytes = pb;
            if (cb(&n, priv)) return depth + 1;
            depth++;
        } else if (tok == FDT_END_NODE) {
            depth--;
            if (depth < 0) break;
            int i = 0;
            while (path[i]) i++;
            if (i > 1) {
                i--;
                while (i > 0 && path[i] != '/') i--;
                path[i] = 0;
            }
        } else if (tok == FDT_NOP) {
        } else if (tok == FDT_END) break;
        else if (tok == FDT_PROP) {
            u32 plen = be32((const u8 *)p);
            p += 2;
            usize adv = (plen + 3) & ~3UL;
            p = (const u32 *)((const u8 *)p + adv);
        }
    }
    return depth;
}

struct comp_priv {
    const char *compat;
    fdt_mmio_t *out;
    int max;
    int count;
    bool want_reg_shift;
};

static bool compat_match(const u8 *val, u32 len, const char *needle)
{
    usize i = 0;
    while (i < len) {
        const char *s = (const char *)(val + i);
        if (str_eq(s, needle)) return true;
        i += str_len(s) + 1;
    }
    return false;
}

static int comp_cb(const walk_node_t *n, void *priv)
{
    struct comp_priv *cp = priv;
    u32 len = 0;
    const u8 *v = find_prop(n, "compatible", &len);
    if (!v || !compat_match(v, len, cp->compat)) return 0;
    if (cp->count >= cp->max) return 0;
    v = find_prop(n, "reg", &len);
    if (!v || len < 16) return 0;
    cp->out[cp->count].base = be64(v);
    v = find_prop(n, "interrupts", &len);
    if (v && len >= 4) cp->out[cp->count].irq = be32(v);
    if (cp->want_reg_shift) {
        cp->out[cp->count].reg_shift = 0;
        v = find_prop(n, "reg-shift", &len);
        if (v && len >= 4) cp->out[cp->count].reg_shift = be32(v);
    } else cp->out[cp->count].reg_shift = 0;
    cp->count++;
    return 0;
}

int fdt_find_uart(fdt_mmio_t *out)
{
    struct comp_priv cp = { "ns16550a", out, 1, 0, true };
    walk(comp_cb, &cp);
    if (cp.count == 0) { cp.compat = "ns16550"; cp.count = 0; walk(comp_cb, &cp); }
    if (cp.count == 0) { cp.compat = "snps,dw-apb-uart"; cp.count = 0; walk(comp_cb, &cp); }
    return cp.count;
}

int fdt_find_virtio(fdt_mmio_t *out, int max)
{
    struct comp_priv cp = { "virtio,mmio", out, max, 0, false };
    walk(comp_cb, &cp);
    return cp.count;
}

int fdt_find_sdhci(fdt_mmio_t *out, int max)
{
    struct comp_priv cp = { "sdhci", out, max, 0, false };
    walk(comp_cb, &cp);
    if (cp.count == 0) { cp.compat = "generic-sdhci"; cp.count = 0; walk(comp_cb, &cp); }
    return cp.count;
}

int fdt_find_clint(paddr_t *out)
{
    fdt_mmio_t tmp;
    struct comp_priv cp = { "riscv,clint0", &tmp, 1, 0, false };
    walk(comp_cb, &cp);
    if (cp.count > 0) { *out = tmp.base; return 1; }
    *out = CLINT_BASE;
    return 1;
}

int fdt_find_plic(paddr_t *out)
{
    fdt_mmio_t tmp;
    struct comp_priv cp = { "riscv,plic0", &tmp, 1, 0, false };
    walk(comp_cb, &cp);
    if (cp.count > 0) { *out = tmp.base; return 1; }
    *out = PLIC_BASE;
    return 1;
}
