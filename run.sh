#!/bin/bash

set -e

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

# Use OVMF images from home (WSL). Copy to build/ so VARS is writable.
OVMF_CODE_SRC="$HOME/ovmf/OVMF_CODE_4M.fd"
OVMF_VARS_SRC="$HOME/ovmf/OVMF_VARS_4M.fd"
OVMF_CODE="build/OVMF_CODE_4M.fd"
OVMF_VARS="build/OVMF_VARS_4M.fd"

print_step "Checking build environment"

# Windows build folder (your project lives on F:)
WIN_BUILD="/mnt/f/Tiny64/build"

# WSL mount point for FAT image
WSL_MOUNT="build/mount"

mkdir -p build
mkdir -p "$WSL_MOUNT"
mkdir -p "$WSL_MOUNT"

print_step "Preparing OVMF firmware"
cp "$OVMF_CODE_SRC" "$OVMF_CODE" || {
    print_error "Missing $OVMF_CODE_SRC"
    exit 1
}
cp "$OVMF_VARS_SRC" "$OVMF_VARS" || {
    print_error "Missing $OVMF_VARS_SRC"
    exit 1
}

print_step "Building project"
MAKE_FLAGS=""
USB_FLAGS="-device qemu-xhci,id=xhci -device usb-mouse,bus=xhci.0 -device usb-kbd,bus=xhci.0"
echo -e "${YELLOW}  [!] USB Enabled (xHCI + Tablet + Keyboard)${NC}"
MAKE_FLAGS="USE_USB=1 DISABLE_PS2=1"

# Default: in QEMU, disable PS/2 unless explicitly needed.
if [[ "$*" == *"--ps2"* ]]; then
    MAKE_FLAGS="${MAKE_FLAGS} DISABLE_PS2=0"
fi

TRIAL_FLAGS=""
if [[ "$*" == *"--trial"* ]] || [[ "$*" == *"--skip-wizard"* ]]; then
    echo -e "${YELLOW}  [!] Trial Mode Flag Set (Skip Wizard)${NC}"
    TRIAL_FLAGS="1"
fi

NET_FLAGS=""

echo -e "${YELLOW}  [!] Networking Enabled (e1000)${NC}"
NET_FLAGS="-netdev user,id=net0 -device e1000,netdev=net0"

for arg in "$@"; do
    case "$arg" in
    --netE)
        echo -e "${YELLOW}  [!] Networking Enabled (e1000e)${NC}"
        NET_FLAGS="-netdev user,id=net0 -device e1000e,netdev=net0"
        ;;
    --no-net)
        echo -e "${YELLOW}  [!] Networking Disabled${NC}"
        NET_FLAGS=""
        ;;
    --no-usb)
        echo -e "${YELLOW}  [!] USB Disabled${NC}"
        USB_FLAGS=""
        MAKE_FLAGS="${MAKE_FLAGS/USE_USB=1/USE_USB=0}"
        ;;
    esac
done

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

# UEFI shell auto-start script
if [ -f "startup.nsh" ]; then
    mcopy -i build/disk.img@@1M startup.nsh ::/startup.nsh
fi

# Copy assets to root
if [ -d "assets" ]; then
    print_step "Copying assets to disk root"
    mcopy -i build/disk.img@@1M assets/* ::/
fi

OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS_TEMPLATE=/usr/share/OVMF/OVMF_VARS_4M.fd

if [ ! -f "$OVMF_CODE" ]; then
    echo "OVMF CODE firmware not found at $OVMF_CODE"
    exit 1
fi

if [ "$TRIAL_FLAGS" == "1" ]; then
    echo "  [+] Adding trial mode trigger"
    echo "" >build/trial_trigger
    mcopy -i build/disk.img@@1M build/trial_trigger ::/TRIAL
    rm build/trial_trigger
fi

print_success "Disk image ready (Synchronized in < 5s)"

echo -e "${GREEN}"
echo "------------------------------------------"
echo "  Launch Successful! Spawning QEMU..."
echo "------------------------------------------"
echo -e "${NC}"

SERIAL_FLAGS="-serial file:build/serial.log"
if [[ "$*" == *"--serial"* ]]; then
    SERIAL_FLAGS="-serial stdio"
fi

qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -drive if=ide,file=build/disk.img,format=raw \
    -net none \
    $NET_FLAGS \
    $SERIAL_FLAGS \
    $USB_FLAGS
