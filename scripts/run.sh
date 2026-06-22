#!/bin/bash
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
KERNEL_DIR="$(cd "$HERE/.." && pwd)"
BOOT_DIR="$(cd "$KERNEL_DIR/../SlipperBoot" && pwd)"
CROSS="${CROSS:-riscv64-elf}"

echo "==> Building SlipperBoot"
make -C "$BOOT_DIR" CROSS="$CROSS" clean all

echo "==> Building SlipperKernel"
make -C "$KERNEL_DIR" CROSS="$CROSS-" clean rust

echo "==> Building host tools"
make -C "$KERNEL_DIR" scripts/elf2spx scripts/mkimage

echo "==> Building kernel + init"
make -C "$KERNEL_DIR" CROSS="$CROSS-" build/kernel.elf build/init.spx

echo "==> Creating raw SlipperFS image"
make -C "$KERNEL_DIR" build/disk.img

echo "==> Creating partitioned disk for SlipperBoot"
DISK="$KERNEL_DIR/build/boot.img"
dd if=/dev/zero of="$DISK" bs=1M count=64 2>/dev/null
parted -s "$DISK" mklabel msdos
parted -s "$DISK" mkpart primary fat32 1MiB 5MiB

FAT_LBA=2048
mkfs.fat -F 32 "$DISK" --offset=$FAT_LBA
mcopy -i "$DISK"@@$((FAT_LBA * 512)) "$KERNEL_DIR/build/kernel.elf" ::kernel.elf

# Write SlipperFS at LBA 10240
SLBA=10240
dd if="$KERNEL_DIR/build/disk.img" of="$DISK" bs=512 seek=$SLBA conv=notrunc 2>/dev/null

echo "==> Starting QEMU"
qemu-system-riscv64 \
    -M virt \
    -m 256M \
    -bios "$BOOT_DIR/bootloader.bin" \
    -drive file="$DISK",format=raw,if=none,id=drive0 \
    -device virtio-blk-device,drive=drive0 \
    -nographic \
    -serial mon:stdio \
    -no-reboot
