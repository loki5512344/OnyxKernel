/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — boot entry point.
 *
 * SlipperBoot hands off:
 *   a0 = hartid (always 0 on QEMU virt uniprocessor)
 *   a1 = pointer to FDT in DRAM
 *
 * We start in M-mode (SlipperBoot was loaded via -bios bootloader.bin and
 * called us with `mret`-free direct jump). Our job here:
 *   1. Save a0/a1.
 *   2. Zero BSS.
 *   3. Set up a stack.
 *   4. Configure PMP:
 *        region 0  -> kernel space (0x80000000..kernel_end, S-mode RWX)
 *        region 1  -> root space  (above kernel_end, S-mode RW, no X)
 *        region 2  -> user space  (0x0..0x80000000, RWX U-mode) [later locked down]
 *        region 3  -> MMIO        (0x0..0x80000000, RWX all)
 *   5. Delegate traps & interrupts to S-mode.
 *   6. Build kernel root page table (identity-mapped), write satp.
 *   7. mret into kmain() running in S-mode.
 *
 * Note: we cannot easily split user vs MMIO via PMP in MVP, so region 2/3
 * are merged. Real user/kernel isolation is enforced by paging (U bit on
 * user PTEs only).
 */
    .section .text.boot, "ax"
    .global _start
_start:
    /* park all harts except hart 0. We use a0 (passed by SlipperBoot) rather
     * than csrr mhartid so this also works when launched in S-mode. */
    bnez    a0, park

    /* Save FDT pointer (a1) and hartid (a0) into globals. */
    la      t1, _saved_hartid
    sd      a0, 0(t1)
    la      t1, _saved_fdt
    sd      a1, 0(t1)

    /* 2) Zero BSS */
    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:

    /* 3) Stack */
    la      sp, __stack_top
    /* keep a16-byte alignment */

    /* 4) Configure PMP.
     *
     * PMP NAPOT encoding: pmpaddr = (base >> 2) | ((size - 1) >> 3)
     *   For a 1GB region at 0x80000000:
     *     pmpaddr = (0x80000000 >> 2) | ((1GB - 1) >> 3)
     *             = 0x20000000 | 0x0FFFFFFF = 0x2FFFFFFF
     *   For a 2GB region at 0x0:
     *     pmpaddr = 0 | ((2GB - 1) >> 3) = 0x0FFFFFFF
     *
     * pmpcfg layout (per 8 bytes):
     *   bit 7  L   (locked)
     *   bit 6  0
     *   bits 5:3  A (match mode: 0=off, 1=TOR, 2=NA4, 3=NAPOT)
     *   bit 2  X
     *   bit 1  W
     *   bit 0  R
     *
     * Region 0: 0x80000000 .. 0xC0000000 (1GB) — DRAM + kernel, S/U RWX.
     * Region 1: 0x0 .. 0x80000000 (2GB) — MMIO, S/U RWX.
     *
     * NOTE: PMP doesn't distinguish S vs U permission bits per region — the
     * R/W/X bits apply to both S and U. Real user/kernel isolation is
     * enforced by Sv39 paging (U bit on user PTEs only).
     */
    /* On RV64, pmpcfg0 is a 64-bit CSR containing 8 byte-sized configs
     * for pmpaddr0..7. pmpcfg1 is illegal on RV64 (pmpcfg2 covers
     * pmpaddr8..15). We pack both region 0 and region 1 configs into
     * pmpcfg0:
     *   byte 0 (region 0) = L|NAPOT|X|W|R = 0x9F
     *   byte 1 (region 1) = L|NAPOT|X|W|R = 0x9F
     *   bytes 2..7         = 0
     * => pmpcfg0 = 0x0000000000009F9F
     */
    li      t0, 0x2FFFFFFF          /* NAPOT: base 0x80000000, size 1GB */
    csrw    pmpaddr0, t0
    li      t0, 0x0FFFFFFF          /* NAPOT: base 0x0, size 2GB */
    csrw    pmpaddr1, t0
    li      t0, 0x9F9F              /* pmpcfg0: two configs L|NAPOT|XWR */
    csrw    pmpcfg0, t0

    /* 5) Delegate exceptions & interrupts to S-mode.
     *    Page faults (load/store/instr), ecall from U-mode, timer/soft/extern. */
    li      t0, (1<<0)  | (1<<2)  | (1<<3)  | (1<<5)  | (1<<7)  | \
                (1<<8)  | (1<<9)  | (1<<12) | (1<<13) | (1<<15)
    csrw    medeleg, t0
    li      t0, (1<<1) | (1<<5) | (1<<9)
    csrw    mideleg, t0

    /* Disable M-mode interrupts; S-mode will enable SIE selectively. */
    csrw    mie, zero

    /* 6) Set up S-mode entry. Paging is NOT enabled here — kmain() will
     *    call pmm_init() (needs FDT) then vmm_init() (needs PMM) to
     *    allocate page tables and install them via satp.
     *
     *    Until then we run in S-mode with satp=BARE (physical addressing),
     *    which is perfectly valid. */

    /* Set mstatus.MPP = supervisor, MPIE = 0 (we don't want M-mode timer).
     * Set mepc = kmain.
     */
    li      t0, (1 << 11)          /* MPP = S (binary 01) */
    csrs    mstatus, t0
    li      t0, (1 << 7)           /* MPIE = 0 */
    csrc    mstatus, t0

    la      t0, kmain
    csrw    mepc, t0

    /* Pass saved hartid / fdt into kmain via a0/a1. */
    la      t0, _saved_hartid
    ld      a0, 0(t0)
    la      t0, _saved_fdt
    ld      a1, 0(t0)

    /* Make sure satp is BARE so we run with physical addressing until vmm_init. */
    csrw satp, zero

    mret

park:
    wfi
    j       park

    .section .data
    .balign 8
_saved_hartid:
    .dword 0
_saved_fdt:
    .dword 0
