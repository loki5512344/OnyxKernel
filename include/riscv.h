/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * SlipperKernel — RISC-V privileged CSR / instruction encodings.
 * References: RISC-V Privileged ISA Spec v1.12 (Sv39 / Sstc optional).
 */
#ifndef SLIPPER_RISCV_H
#define SLIPPER_RISCV_H

#include "types.h"

/* ---- mstatus / sstatus bits ---- */
#define MSTATUS_MIE    (1UL << 3)
#define MSTATUS_SIE    (1UL << 1)
#define MSTATUS_UIE    (1UL << 0)
#define MSTATUS_MPP_M  (3UL << 11)
#define MSTATUS_MPP_S  (1UL << 11)
#define MSTATUS_MPP_U  (0UL << 11)
#define MSTATUS_SPP_S  (1UL << 8)
#define MSTATUS_SPP_U  (0UL << 8)
#define MSTATUS_MXR    (1UL << 19)
#define MSTATUS_SUM    (1UL << 18)
#define MSTATUS_TVM    (1UL << 20)

#define SSTATUS_SIE    (1UL << 1)
#define SSTATUS_SPIE   (1UL << 5)
#define SSTATUS_SPP    (1UL << 8)
#define SSTATUS_SUM    (1UL << 18)
#define SSTATUS_MXR    (1UL << 19)

/* ---- satp (Sv39) ---- */
#define SATP_MODE_SV39 (8UL << 60)
#define SATP_MODE_BARE (0UL)
#define SATP_PPN_MASK  ((1UL << 44) - 1)

/* ---- scause / mcause codes (high bit = interrupt) ---- */
#define SCAUSE_INT        (1UL << 63)
#define SCAUSE_CODE_MASK  ((1UL << 63) - 1)

#define CAUSE_IAMISS      0
#define CAUSE_LDAMISS     5
#define CAUSE_STAMISS     7
#define CAUSE_U_ECALL     8
#define CAUSE_S_ECALL     9
#define CAUSE_M_ECALL     11
#define CAUSE_ILL         2
#define CAUSE_BRK         3
#define CAUSE_LD_PF       13
#define CAUSE_ST_PF       15
#define CAUSE_INST_PF     12

/* Interrupt causes (with SCAUSE_INT bit) */
#define INTR_S_SOFT       (SCAUSE_INT | 1)
#define INTR_S_TIMER      (SCAUSE_INT | 5)
#define INTR_S_EXTERN     (SCAUSE_INT | 9)

/* ---- medeleg / mideleg bits ---- */
#define MEDELEG_IAMISS   (1U << 0)
#define MEDELEG_LDAMISS  (1U << 5)
#define MEDELEG_STAMISS  (1U << 7)
#define MEDELEG_U_ECALL  (1U << 8)
#define MEDELEG_LD_PF    (1U << 13)
#define MEDELEG_ST_PF    (1U << 15)
#define MEDELEG_INST_PF  (1U << 12)

#define MIDELEG_S_SOFT   (1U << 1)
#define MIDELEG_S_TIMER  (1U << 5)
#define MIDELEG_S_EXTERN (1U << 9)

/* ---- PMP ---- */
#define PMPCFG_OFF   0x3A0
#define PMPCFG_L     (1U << 7)   /* locked */
#define PMPCFG_A     (3U << 3)   /* match mode: NAPOT=3, TOR=1, NA4=2 */
#define PMPCFG_X     (1U << 2)
#define PMPCFG_W     (1U << 1)
#define PMPCFG_R     (1U << 0)

/* pmpaddr is NAPOT-encoded: a/i=1 means address bits, pattern is "(addr)/(size)-1" */
#define PMP_NAPOT(addr, size) (((addr) >> 2) | (((size) - 1) >> 3))

/* ---- Page table entry (Sv39) ---- */
#define PTE_V   (1UL << 0)
#define PTE_R   (1UL << 1)
#define PTE_W   (1UL << 2)
#define PTE_X   (1UL << 3)
#define PTE_U   (1UL << 4)
#define PTE_G   (1UL << 5)
#define PTE_A   (1UL << 6)
#define PTE_D   (1UL << 7)

/* leaf PTE; otherwise it is a pointer to next-level table */
#define PTE_LEAF (PTE_R | PTE_W | PTE_X)

#define PTE_PPN_SHIFT 10
#define PTE_PPN_MASK  ((1UL << 44) - 1)
#define PTE_FLAGS_MASK 0x3FFUL

/* Sv39 has 3 levels of 9-bit indexing */
#define SV39_LEVELS   3
#define SV39_PTES_PT  512
#define SV39_BITS_LV  9
#define SV39_VA_BITS  39
#define SV39_L2_IDX(va)  (((va) >> 30) & 0x1FF)
#define SV39_L1_IDX(va)  (((va) >> 21) & 0x1FF)
#define SV39_L0_IDX(va)  (((va) >> 12) & 0x1FF)

/* CLINT layout for QEMU virt */
#define CLINT_BASE      0x02000000UL
#define CLINT_MTIMECMP  (CLINT_BASE + 0x4000)
#define CLINT_MTIME     (CLINT_BASE + 0xBFF8)
#define CLINT_STIMECMP  (CLINT_BASE + 0xD000)   /* per-hart Sstc, 0x14 per hart */

/* ---- inline CSR access ---- */
#define csr_read(csr) ({ \
    usize __v; \
    asm volatile("csrr %0, " #csr : "=r"(__v)); \
    __v; \
})

#define csr_write(csr, v) ({ \
    asm volatile("csrw " #csr ", %0" :: "rK"(v)); \
})

#define csr_set(csr, mask) ({ \
    asm volatile("csrs " #csr ", %0" :: "rK"(mask)); \
})

#define csr_clear(csr, mask) ({ \
    asm volatile("csrc " #csr ", %0" :: "rK"(mask)); \
})

#define sfence_vma_all()  asm volatile("sfence.vma zero, zero" ::: "memory")
#define sfence_vma(va, asid) asm volatile("sfence.vma %0, %1" :: "r"(va), "r"(asid) : "memory")

/* PLIC (QEMU virt) */
#define PLIC_BASE         0x0C000000UL
#define PLIC_PRIORITY(n)  (PLIC_BASE + 4*(n))
#define PLIC_ENABLE(hart) (PLIC_BASE + 0x2000 + 0x80*(hart))
#define PLIC_THRESHOLD(h) (PLIC_BASE + 0x200000 + 0x1000*(h))
#define PLIC_CLAIM(h)     (PLIC_BASE + 0x200004 + 0x1000*(h))
#define PLIC_COMPLETE(h)  PLIC_CLAIM(h)

#endif
