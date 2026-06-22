// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SlipperKernel — process / task management.
 *
 * MVP: one user process at a time. proc_enter_user() does the S->U drop.
 */
#include "types.h"
#include "proc.h"
#include "trap.h"
#include "vmm.h"
#include "klog.h"
#include "riscv.h"
#include "vfs.h"

static proc_t g_procs[4];
static proc_t *g_current = NULL;
static u32 g_next_pid = 1;

int proc_init(void)
{
    for (int i = 0; i < (int)ARR_LEN(g_procs); ++i) {
        g_procs[i].state = 0;
        g_procs[i].pid = 0;
    }
    g_next_pid = PROC_PID_INIT;
    return 0;
}

proc_t *proc_current(void) { return g_current; }

proc_t *proc_by_pid(u32 pid)
{
    for (int i = 0; i < (int)ARR_LEN(g_procs); ++i)
        if (g_procs[i].pid == pid) return &g_procs[i];
    return NULL;
}

int proc_create_user(vaddr_t entry, vaddr_t ustack, paddr_t root_pa, u32 pid)
{
    proc_t *p = NULL;
    for (int i = 0; i < (int)ARR_LEN(g_procs); ++i) {
        if (g_procs[i].state == 0) { p = &g_procs[i]; break; }
    }
    if (!p) return SL_ERR_NOMEM;

    /* zero trap frame */
    u8 *tfb = (u8 *)&p->tf;
    for (usize i = 0; i < sizeof(trap_frame_t); ++i) tfb[i] = 0;

    p->pid     = pid;
    p->ring    = PROC_RING_USER;
    p->state   = 1;
    p->root_pa = root_pa;
    p->entry   = entry;
    p->ustack  = ustack;
    p->tf.sepc = entry;
    p->tf.sp   = ustack;
    p->tf.a0   = 0;        /* argc */
    p->tf.a1   = ustack - 256;  /* argv pointer area (rough) */

    return 0;
}

__attribute__((noreturn))
void proc_enter_user(u32 pid)
{
    proc_t *p = proc_by_pid(pid);
    if (!p) kpanic("proc_enter_user: no such pid %u", pid);
    g_current = p;

    kinf("proc: entering user pid=%u entry=0x%lx", p->pid, p->entry);

    /* Snapshot all fields we need BEFORE touching sp. The compiler may spill
     * `p' to the stack around calls; if we change sp first and the compiler
     * later reloads p from the old stack offset (now pointing into kstack
     * memory), we'd pass garbage to drop_to_user.  Reading everything here
     * into locals avoids that. */
    vaddr_t entry   = p->entry;
    vaddr_t ustack  = p->ustack;
    paddr_t root_pa = p->root_pa;
    usize kstack_top = (usize)&p->kstack + sizeof(p->kstack);
    kstack_top &= ~15UL;     /* 16-byte align */

    /* Switch to the task's kernel stack. drop_to_user() records this sp
     * into sscratch so the next trap entry has a kernel stack. */
    asm volatile("mv sp, %0" : : "r"(kstack_top) : "memory");

    /* Drop to user mode. drop_to_user never returns. */
    drop_to_user(entry, ustack, root_pa);
    kpanic("proc_enter_user: drop_to_user returned");   /* unreachable */
}

__attribute__((noreturn))
void proc_exit(u32 pid, int code)
{
    proc_t *p = proc_by_pid(pid);
    if (p) {
        kinf("proc: pid %u exited with code %d", pid, code);
        if (p->root_pa) vmm_destroy_root(p->root_pa);
        p->state = 3;
    } else {
        kerr("proc: exit unknown pid %u", pid);
    }
    /* No other process to switch to in MVP. Halt. */
    kinf("proc: no more processes, halting");
    khalt();
}
