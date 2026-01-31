# Tiny64 OS

![Tiny64 Logo](assets/images/Tiny64White.png)

A sophisticated 64-bit UEFI Operating System written in C, C++, and Assembly. Tiny64 combines low-level hardware control with a modern C++ desktop environment.

## 🚀 Overview

Tiny64 is a research operating system designed to demonstrate the power of modern 64-bit computing on UEFI hardware. It features a custom bootloader, a high-performance kernel, and a feature-rich graphical user interface.

## ✨ Key Features

### 🖥️ Graphical User Interface (C++)
- **Desktop Environment**: A fully functional desktop with a taskbar, start menu, and wallpaper support.
- **Window Management**: Overlapping windows with focus management, minimizing, and dragging.
- **Terminal**: A built-in terminal window for system interaction.
- **File Explorer**: Browse the filesystem with a modern icon-based interface (powered by `FileExplorer.png`).
- **Rendering Engine**: Advanced software renderer supporting:
    - Rounded rectangles and transparency.
    - Multiple font systems (Bitmap 8x16, Vector fonts, and High-quality TTF rendering using **Inter-Regular.ttf** via `stb_truetype`).
    - Smooth cursor rendering and double-buffering.

## 🎨 Visual Identity

Tiny64 features a clean, professional aesthetic:

| Light Theme Icon | Dark Theme Icon | Explorer UI |
|:---:|:---:|:---:|
| ![Tiny64 White](assets/images/Tiny64White.png) | ![Tiny64 Black](assets/images/Tiny64Black.png) | ![File Explorer](assets/images/FileExplorer.png) |

### 🔌 Advanced Driver Stack
- **USB 3.0 (xHCI)**: Complete driver for modern USB controllers supporting Keyboards, Mice, and Mass Storage.
- **Storage**: 
    - **NVMe**: High-speed flash storage support.
    - **AHCI**: SATA controller support for HDDs/SSDs.
    - **Legacy ATA**: Fallback support for older IDE systems.
- **Networking**: Intel e1000 Gigabit Ethernet driver.
- **HID**: Support for both modern USB HID and legacy PS/2 input.
- **PCI**: Dynamic bus scanning and configuration.

### 🧠 Kernel Core
- **Architecture**: Pure 64-bit (x86_64) long mode.
- **Multitasking**: Preemptive scheduler with task switching.
- **Memory Management**: 
    - Physical Memory Manager (PMM) based on UEFI memory map.
    - SLAB-style Heap allocator for dynamic C++ objects.
- **Interrupts**: Full IDT implementation with remapped PIC/IOAPIC support.
- **Filesystem**: 
    - **RamFS**: High-speed in-memory filesystem.
    - **FAT32**: Support for standard disk partitions.

## 🛠️ Build & Run

### Prerequisites
Tiny64 is designed to be built on Linux or WSL2.

```bash
sudo apt-get update
sudo apt-get install \
    build-essential \
    gcc-mingw-w64-x86-64 \
    qemu-system-x86 \
    ovmf \
    dosfstools \
    mtools \
    gdisk
```

### Building
```bash
# Clean and build the entire project
make clean && make
```

### Running (QEMU)
The `run.sh` script handles disk image creation and QEMU execution:

```bash
# Standard boot
./run.sh

# Boot with USB (xHCI) enabled
./run.sh --usb

# Boot with Networking (e1000) enabled
./run.sh --net

# Boot directly into Recovery Mode
./run.sh --recovery
```

## 📂 Project Structure

```
Tiny64/
├── assets/             # Visual assets (Logos, Fonts, Icons)
├── boot/               # UEFI Native Bootloader
│   ├── c/              # Bootloader source (PE32+ via MinGW)
│   └── include/        # UEFI system headers
├── kernel/             # 64-bit Kernel
│   ├── asm/            # Low-level assembly (Entry, GDT, Interrupts)
│   ├── c/              # Kernel C core (Drivers, Memory, FS, Graphics)
│   └── cpp/            # Kernel C++ layer (UI, Window Manager, Utils)
├── tools/              # Deployment and asset conversion scripts
├── OVMF/               # UEFI Firmware files
├── Makefile            # Global build system
└── run.sh              # Advanced deployment script
```

## 🛠️ Technical Stack
- **Languages**: C11, C++17, x86_64 Assembly.
- **Bootloader**: UEFI Specification 2.x.
- **Format**: PE32+ (Bootloader), ELF64 (Kernel).
- **Toolchain**: GCC (Kernel), MinGW (Bootloader), GNU Linker.

## 📜 License
This project is provided for educational and research purposes.