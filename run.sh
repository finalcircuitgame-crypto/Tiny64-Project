#!/bin/bash

set -e

print_banner() {
    echo -e "${CYAN}"
    echo "  _______ _             __   _  _   "
    echo " |__   __(_)           / /  | || |  "
    echo "    | |   _ _ __  _   / /_  | || |_ "
    echo "    | |  | | '_ \\| | | '_ \\ |__   _|"
    echo "    | |  | | | | | |_| (_) |   | |  "
    echo "    |_|  |_|_| |_|\\__, \\___/    |_|  "
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

# Always run from the repository root, regardless of the caller's cwd.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Keep a writable local copy of OVMF VARS; use the matching CODE image.
OVMF_CODE_SRC="$HOME/ovmf/OVMF_CODE_4M.fd"
OVMF_VARS_SRC="$HOME/ovmf/OVMF_VARS_4M.fd"
OVMF_CODE="build/OVMF_CODE_4M.fd"
OVMF_VARS="build/OVMF_VARS_4M.fd"

print_step "Checking build environment"

# Windows build output is kept alongside this repository on the F: drive.
WIN_BUILD="$SCRIPT_DIR/build"
WSL_MOUNT="build/mount"

mkdir -p build "$WSL_MOUNT"

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

MAKE_FLAGS="USE_USB=1 DISABLE_PS2=1"
USB_ENABLED=1
USB_POINTER_DEVICE="usb-mouse"

# USB mouse is the default because the current HID path handles relative
# mouse reports directly. Use --tablet to opt into QEMU's absolute tablet.
for arg in "$@"; do
    case "$arg" in
        --ps2)
            MAKE_FLAGS="${MAKE_FLAGS} DISABLE_PS2=0"
            ;;
        --tablet)
            USB_POINTER_DEVICE="usb-tablet"
            ;;
        --trial|--skip-wizard)
            TRIAL_FLAGS="1"
            ;;
        --no-usb)
            USB_ENABLED=0
            MAKE_FLAGS="${MAKE_FLAGS/USE_USB=1/USE_USB=0}"
            ;;
    esac
done

if [[ "$USB_ENABLED" == "1" ]]; then
    USB_FLAGS=(
        -device qemu-xhci,id=xhci
        -device "${USB_POINTER_DEVICE},bus=xhci.0"
        -device usb-kbd,bus=xhci.0
    )
    echo -e "${YELLOW}  [!] USB Enabled (xHCI + ${USB_POINTER_DEVICE} + Keyboard)${NC}"
else
    USB_FLAGS=()
    echo -e "${YELLOW}  [!] USB Disabled${NC}"
fi

# QEMU network arguments are mutually exclusive: do not combine -net none
# with an explicitly configured network backend/device.
NET_FLAGS=(-netdev user,id=net0 -device e1000,netdev=net0)
for arg in "$@"; do
    case "$arg" in
        --netE)
            echo -e "${YELLOW}  [!] Networking Enabled (e1000e)${NC}"
            NET_FLAGS=(-netdev user,id=net0 -device e1000e,netdev=net0)
            ;;
        --no-net)
            echo -e "${YELLOW}  [!] Networking Disabled${NC}"
            NET_FLAGS=(-net none)
            ;;
    esac
done

TRIAL_FLAGS="${TRIAL_FLAGS:-}"
if [[ "$TRIAL_FLAGS" == "1" ]]; then
    echo -e "${YELLOW}  [!] Trial Mode Flag Set (Skip Wizard)${NC}"
fi

make $MAKE_FLAGS || {
    print_error "Build failed"
    exit 1
}
print_success "Build complete"

print_step "Creating Optimized Disk Image (64MB)"
qemu-img create -f raw build/disk.img 64M >/dev/null

print_step "Configuring GPT partitions"
sgdisk -Z build/disk.img >/dev/null
sgdisk -n 1:2048:131038 -t 1:EF00 -c 1:"EFI System Partition" build/disk.img >/dev/null

print_step "Formatting & Copying Files (High Speed)"
mformat -i build/disk.img@@1M -F -v "TINY64" >/dev/null

mmd -i build/disk.img@@1M ::/EFI
mmd -i build/disk.img@@1M ::/EFI/BOOT

# These are the boot artifacts produced for the desktop build.
BOOTLOADER_IMAGE="$WIN_BUILD/boot/BOOTX64.EFI"
KERNEL_IMAGE="$WIN_BUILD/kernel/kernel.elf"
if [[ ! -f "$BOOTLOADER_IMAGE" || ! -f "$KERNEL_IMAGE" ]]; then
    print_error "Missing boot artifacts under $WIN_BUILD"
    echo "Expected:"
    echo "  $BOOTLOADER_IMAGE"
    echo "  $KERNEL_IMAGE"
    exit 1
fi

mcopy -o -i build/disk.img@@1M "$BOOTLOADER_IMAGE" ::/EFI/BOOT/BOOTX64.EFI
mcopy -o -i build/disk.img@@1M "$KERNEL_IMAGE" ::/kernel.elf

if [ -f "startup.nsh" ]; then
    mcopy -o -i build/disk.img@@1M startup.nsh ::/startup.nsh
fi

if [ -d "assets" ]; then
    print_step "Copying assets to disk root"
    for asset in assets/*; do
        [[ -e "$asset" ]] || continue
        mcopy -o -i build/disk.img@@1M "$asset" ::/
    done
fi

if [[ "${TRIAL_FLAGS:-}" == "1" ]]; then
    echo "" > build/trial_trigger
    mcopy -o -i build/disk.img@@1M build/trial_trigger ::/TRIAL
    rm build/trial_trigger
fi

print_success "Disk image ready"

echo -e "${GREEN}"
echo "------------------------------------------"
echo "  Launch Successful! Spawning QEMU..."
echo "------------------------------------------"
echo -e "${NC}"

if [[ " $* " == *" --serial "* ]]; then
    SERIAL_FLAGS=(-serial stdio)
else
    SERIAL_FLAGS=(-serial file:build/serial.log)
fi

qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -drive if=ide,file=build/disk.img,format=raw \
    "${NET_FLAGS[@]}" \
    "${SERIAL_FLAGS[@]}" \
    "${USB_FLAGS[@]}"
