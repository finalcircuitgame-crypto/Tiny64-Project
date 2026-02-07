#!/bin/bash
# Path to OVMF firmware

make all

OVMF_PATH=/usr/share/ovmf/OVMF.fd
if [ ! -f "$OVMF_PATH" ]; then
    OVMF_PATH=/usr/share/qemu/OVMF.fd
fi

qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_PATH" \
    -drive format=raw,file=dist/tiny64.img \
    -net none \
    -serial stdio