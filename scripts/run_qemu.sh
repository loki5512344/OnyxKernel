#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# run_qemu.sh — build & run SlipperKernel under QEMU.
#
# Assumes SlipperBoot is checked out at $SLIPPERBOOT_DIR (default: ../SlipperBoot)
# and that its `bootloader.bin` is built.
#
# QEMU options:
#   -M virt         standard RISC-V virtual platform
#   -m 256M         256 MB DRAM (kernel + heap live in upper part)
#   -bios bootloader.bin   SlipperBoot is the firmware, runs in M-mode
#   -drive ...      raw disk image with SlipperFS, attached as virtio-blk
#   -nographic      no GUI, use serial console

set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BOOT_DIR="${SLIPPERBOOT_DIR:-$ROOT/../SlipperBoot}"
BOOT_BIN="$BOOT_DIR/bootloader.bin"

if [ ! -f "$BOOT_BIN" ]; then
    echo "[run_qemu] bootloader.bin not found at $BOOT_BIN"
    echo "[run_qemu] build SlipperBoot first:  (cd $BOOT_DIR && make)"
    exit 1
fi

if [ ! -f "$ROOT/build/kernel.elf" ]; then
    echo "[run_qemu] kernel.elf not found, building..."
    make -C "$ROOT" kernel.elf
fi
if [ ! -f "$ROOT/build/init.spx" ]; then
    echo "[run_qemu] init.spx not found, building..."
    make -C "$ROOT" init.spx
fi
if [ ! -f "$ROOT/build/disk.img" ]; then
    echo "[run_qemu] disk.img not found, building..."
    make -C "$ROOT" disk.img
fi

DISK="$ROOT/build/disk.img"

qemu-system-riscv64 \
    -M virt \
    -m 256M \
    -bios "$BOOT_BIN" \
    -drive file="$DISK",format=raw,if=none,id=drive0 \
    -device virtio-blk-device,drive=drive0 \
    -nographic \
    -serial mon:stdio \
    -no-reboot
