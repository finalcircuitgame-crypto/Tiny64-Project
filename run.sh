#!/bin/bash

# Define variables
OUT_DIR="build"
IMG_FILE="$OUT_DIR/os.img"
EFI_DIR="EFI/BOOT"

mkdir -p "$OUT_DIR"

echo "Compiling UEFI loader..."
x86_64-w64-mingw32-gcc -nostdlib -ffreestanding -Wl,-dll -Wl,--subsystem,10 \
    -e efi_main src/loader.c -o "$OUT_DIR/BOOTX64.EFI" -mno-sse

if [ $? -ne 0 ]; then
    echo "UEFI loader build failed."
    exit 1
fi

echo "Compiling Tiny64 kernel..."
# Compile pmm.c and kernel.c
x86_64-w64-mingw32-gcc -c src/pmm.c -o "$OUT_DIR/pmm.o" -ffreestanding -O2 -mno-red-zone -mno-sse \
    -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables
x86_64-w64-mingw32-gcc -c src/kernel.c -o "$OUT_DIR/kernel.o" -ffreestanding -O2 -mno-red-zone -mno-sse \
    -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables

# Link kernel with pmm.o using custom linker script
x86_64-w64-mingw32-gcc -nostdlib -ffreestanding -T src/kernel.ld "$OUT_DIR/kernel.o" "$OUT_DIR/pmm.o" -o "$OUT_DIR/kernel.tmp"

# Extract raw binary
x86_64-w64-mingw32-objcopy -O binary "$OUT_DIR/kernel.tmp" "$OUT_DIR/kernel.bin"

if [ $? -ne 0 ]; then
    echo "Kernel build failed."
    exit 1
fi

echo "Creating FAT32 disk image..."
dd if=/dev/zero of="$IMG_FILE" bs=1M count=64
mkfs.fat -F 32 "$IMG_FILE"

mmd -i "$IMG_FILE" ::/EFI
mmd -i "$IMG_FILE" ::/EFI/BOOT
mcopy -i "$IMG_FILE" "$OUT_DIR/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$IMG_FILE" "$OUT_DIR/kernel.bin" ::/kernel.bin

echo "Build successful: $IMG_FILE is ready."

# Run in QEMU with UEFI firmware
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive format=raw,file="$IMG_FILE" \
    -net none \
    -serial stdio
