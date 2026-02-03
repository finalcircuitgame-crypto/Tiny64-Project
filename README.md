# Tiny64 OS

Tiny64 is a visually polished 64-bit UEFI hobby operating system written in C and C++. It features high-quality TTF font rendering, a responsive GUI desktop, and modern hardware support including USB 3.x (xHCI).

## ✨ Key Features

- **High-Quality Graphics**: 
  - TTF Font Rendering (Roboto & Inter) using safe glyph caching.
  - Flicker-free Double Buffering.
  - C++ GUI Desktop with a live clock, taskbar, and wallpaper.
- **Robust Terminal**:
  - 4096-line history buffer.
  - Logical line wrapping and smooth scrolling.
  - Synchronized window boundaries with the GUI environment.
- **Hardware & Drivers**:
  - **USB 3.x (xHCI)**: Full host controller driver with HID keyboard support.
  - **Networking**: Intel e1000 driver with ARP/UDP support (includes ARP timeout stability).
  - **RTC**: CMOS-based Real Time Clock for live time updates.
  - **PCI**: Dynamic device enumeration.
- **UEFI Native**: Boots directly via UEFI (no legacy BIOS mess).

## 🚀 Getting Started

### Prerequisites

You will need a cross-compilation environment (standard in most Linux distros or WSL):
- `gcc` / `g++` (x86_64-w64-mingw32 or standard x64 cross-compiler)
- `make`
- `mtools` (for FAT image creation)
- `qemu-system-x86_64` (for testing)

### Building

To build the kernel and create the bootable image:

```bash
make image
```

This generates `build/disk.img`.

### Running in QEMU

You can run the OS in emulation with:

```bash
make run
```

*Note: The Makefile is configured to support the USB keyboard and xHCI controller in QEMU.*

## 💾 Deploying to Hardware

We provide a convenient GUI tool to deploy Tiny64 to a real USB drive:

1. Locate `deploy_to_usb.ps1` in the root directory.
2. **Right-click** the file and select **"Run with PowerShell"**.
3. Select your USB drive and click **ERASE & DEPLOY**.

The script will automatically:
- Clear the drive (with confirmation).
- Create the standard `EFI/BOOT/` structure.
- Copy `BOOTX64.EFI` and `kernel.elf` to their correct locations.
- Detect and warn you if QEMU or other programs are locking the files.

## 🛠️ Project Structure

- `arch/x86/`: Kernel entry point and architecture-specific code.
- `drivers/`: Hardware drivers (USB/xHCI, Network/e1000, RTC, PCI).
- `kernel/`: Core kernel logic, Memory Management (PMM/Heap), and Console.
- `kernel/gui/`: C++ based desktop environment.
- `include/`: Header files for all components.
- `lib/`: Shared libraries (String, TTF rendering, etc.).

---

Developed with ❤️ as an exploration of modern OS design on x86_64.
