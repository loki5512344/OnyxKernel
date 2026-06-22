/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — trap frame + entry/exit glue.
 */
#ifndef SLIPPER_TRAP_H
#define SLIPPER_TRAP_H

#include "types.h"

typedef struct {
    /* Saved by trap.S on the kernel stack of the current task. */
    u64 ra, sp, gp, tp;
    u64 t0, t1, t2;
    u64 s0, s1;
    u64 a0, a1, a2, a3, a4, a5, a6, a7;
    u64 s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    u64 t3, t4, t5, t6;
    /* Saved CSRs (read from s* CSR after entry). */
    u64 sepc;
    u64 sstatus;
    u64 scause;
    u64 stval;
    u64 satp;        /* user root satp at entry — restored on sret */
} trap_frame_t;

void trap_entry(void);          /* asm; installed as stvec */
void trap_handler(trap_frame_t *f);

/* Set stvec to point at trap_entry. */
void trap_init(void);

/* Drop to U-mode at given entry va with given user stack pointer.
 * Used by spx loader to start /bin/init. */
__attribute__((noreturn))
void drop_to_user(vaddr_t entry, vaddr_t ustack, paddr_t user_root_pa);

#endif
