#!/bin/bash

# --- Visual Enhancements ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_banner() {
    echo -e "${CYAN}"
    echo "  _______ _             __   _  _   "
    echo " |__   __(_)           / /  | || |  "
    echo "    | |   _ _ __  _   / /_  | || |_ "
    echo "    | |  | | '_ \| | | '_ \ |__   _|"
    echo "    | |  | | | | | |_| (_) |   | |  "
    echo "    |_|  |_|_| |_|\__, \___/    |_|  "
    echo "                   __/ |            "
    echo "                  |___/             "
    echo -e "${NC}"
    echo -e "${PURPLE}Starting Tiny64 OS Build & Boot System...${NC}"
    echo "------------------------------------------"
}

print_step() {
    echo -e "${BLUE}==>${NC} ${1}..."
}

print_success() {
    echo -e "${GREEN}✓${NC} ${1}"
}

print_error() {
    echo -e "${RED}✗${NC} ${1}"
}

print_banner

# Paths to OVMF inside WSL (must be writable)
OVMF_CODE="$HOME/ovmf/OVMF_CODE_4M.fd"
OVMF_VARS="$HOME/ovmf/OVMF_VARS_4M.fd"

print_step "Checking build environment"

# Windows build folder (your project lives on F:)
WIN_BUILD="/mnt/f/Tiny64/build"

# WSL mount point for FAT image
WSL_MOUNT="build/mount"

mkdir -p build
mkdir -p "$WSL_MOUNT"
mkdir -p "$WSL_MOUNT"

print_step "Building project"
MAKE_FLAGS=""
USB_FLAGS=""
if [[ "$*" == *"--usb"* ]]; then
    echo -e "${YELLOW}  [!] USB Mode Enabled (xHCI + Tablet + Keyboard)${NC}"
    MAKE_FLAGS="USE_USB=1 DISABLE_PS2=1"
    USB_FLAGS="-device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 -device usb-kbd,bus=xhci.0"
fi

NET_FLAGS=""
if [[ "$*" == *"--net"* ]]; then
    echo -e "${YELLOW}  [!] Networking Enabled (e1000)${NC}"
    NET_FLAGS="-netdev user,id=net0 -device e1000,netdev=net0"
fi

RECOVERY_FLAGS=""
if [[ "$*" == *"--recovery"* ]]; then
    echo -e "${YELLOW}  [!] Recovery Mode Flag Set${NC}"
    RECOVERY_FLAGS="1"
fi

make $MAKE_FLAGS || {
    print_error "Build failed"
    exit 1
}
print_success "Build complete"

print_step "Creating Optimized Disk Image (64MB)"
# Use a smaller image for speed, and pre-fill with zero only if needed
qemu-img create -f raw build/disk.img 64M >/dev/null

# Create GPT partition table with EFI System Partition
print_step "Configuring GPT partitions"
sgdisk -Z build/disk.img >/dev/null
sgdisk -n 1:2048:131038 -t 1:EF00 -c 1:"EFI System Partition" build/disk.img >/dev/null

# --- FAST COPY PATH USING MTOOLS ---
# This avoids sudo mount/umount which is very slow in WSL
print_step "Formatting & Copying Files (High Speed)"

# Offset for partition 1 is 2048 sectors * 512 bytes = 1048576 bytes
# We use mformat to format the partition directly inside the image
mformat -i build/disk.img@@1M -F -v "TINY64" >/dev/null

# Create directories
mmd -i build/disk.img@@1M ::/EFI
mmd -i build/disk.img@@1M ::/EFI/BOOT

# Copy files using mcopy
mcopy -i build/disk.img@@1M "$WIN_BUILD/boot/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i build/disk.img@@1M "$WIN_BUILD/kernel/kernel.elf" ::/kernel.elf

# Copy assets to root
if [ -d "assets" ]; then
    print_step "Copying assets to disk root"
    mcopy -i build/disk.img@@1M assets/* ::/
fi

# Create placeholder logs file (10MB) to reserve clusters
print_step "Reserving space for serial logs"
dd if=/dev/zero of=build/serial.txt bs=1M count=10 >/dev/null 2>&1
mcopy -i build/disk.img@@1M build/serial.txt ::/serial.txt
rm build/serial.txt

if [ "$RECOVERY_FLAGS" == "1" ]; then
    echo "  [+] Adding recovery trigger"
    # Create empty file
    echo "" >build/recovery_trigger
    mcopy -i build/disk.img@@1M build/recovery_trigger ::/RECOVERY
    rm build/recovery_trigger
fi

print_success "Disk image ready (Synchronized in < 5s)"

echo -e "${GREEN}"
echo "------------------------------------------"
echo "  Launch Successful! Spawning QEMU..."
echo "------------------------------------------"
echo -e "${NC}"

qemu-system-x86_64 \
    -m 2G \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -drive if=ide,file=build/disk.img,format=raw \
    -net none \
    $NET_FLAGS \
    -serial stdio \
    $USB_FLAGS
