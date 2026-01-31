# Toolchain
UEFI_CC := x86_64-w64-mingw32-gcc
CC := gcc
LD := ld
AS := as
OBJCOPY := objcopy

# Directories
BUILD_DIR := build
BOOT_DIR := boot
KERNEL_DIR := kernel

# Bootloader sources
BOOT_SRC_C := $(shell find $(BOOT_DIR)/c -name '*.c')

BOOT_OBJS := \
	$(patsubst $(BOOT_DIR)/c/%.c,$(BUILD_DIR)/boot/c/%.o,$(BOOT_SRC_C))

BOOT_EFI := $(BUILD_DIR)/boot/BOOTX64.EFI

# Flags
USE_USB ?= 0
USE_QEMU_XHCI ?= 0
DISABLE_PS2 ?= 0
BOOT_CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -g -Iboot/include -fno-stack-check -fno-asynchronous-unwind-tables

# Kernel sources
KERNEL_SRC_C := $(shell find $(KERNEL_DIR)/c -name '*.c')
XHCI_INCLUDE := -I$(KERNEL_DIR)/c/drivers/usb/xhci -I$(KERNEL_DIR)/c/drivers/usb/xhci/qemu

# Doom Generic Sources
DOOM_DIR := external/doomgeneric
DOOM_SRC := $(filter-out $(DOOM_DIR)/doomgeneric_allegro.c \
                        $(DOOM_DIR)/doomgeneric_emscripten.c \
                        $(DOOM_DIR)/doomgeneric_linuxvt.c \
                        $(DOOM_DIR)/doomgeneric_sdl.c \
                        $(DOOM_DIR)/doomgeneric_soso.c \
                        $(DOOM_DIR)/doomgeneric_sosox.c \
                        $(DOOM_DIR)/doomgeneric_win.c \
                        $(DOOM_DIR)/doomgeneric_xlib.c \
                        $(DOOM_DIR)/i_allegromusic.c \
                        $(DOOM_DIR)/i_allegrosound.c \
                        $(DOOM_DIR)/i_sdlmusic.c \
                        $(DOOM_DIR)/i_sdlsound.c \
                        $(DOOM_DIR)/i_cdmus.c \
                        $(DOOM_DIR)/i_joystick.c \
                        $(DOOM_DIR)/dummy.c, \
                        $(wildcard $(DOOM_DIR)/*.c))

KERNEL_SRC_CPP := $(shell find $(KERNEL_DIR)/cpp -name '*.cpp')
KERNEL_SRC_ASM := $(shell find $(KERNEL_DIR)/asm -name '*.S')

KERNEL_OBJS := \
	$(patsubst $(KERNEL_DIR)/c/%.c,$(BUILD_DIR)/kernel/c/%.o,$(KERNEL_SRC_C)) \
	$(patsubst $(DOOM_DIR)/%.c,$(BUILD_DIR)/kernel/doom/%.o,$(DOOM_SRC)) \
	$(BUILD_DIR)/kernel/c/core/commands.o \
	$(BUILD_DIR)/kernel/cpp/ui/doom.o \
	$(patsubst $(KERNEL_DIR)/cpp/%.cpp,$(BUILD_DIR)/kernel/cpp/%.o,$(KERNEL_SRC_CPP)) \
	$(patsubst $(KERNEL_DIR)/asm/%.S,$(BUILD_DIR)/kernel/asm/%.o,$(KERNEL_SRC_ASM))

KERNEL_BIN := $(BUILD_DIR)/kernel/kernel.elf
KERNEL_LDS := $(KERNEL_DIR)/config/linker/kernel.ld

CFLAGS := -O3 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-red-zone -D_FORTIFY_SOURCE=0 -fno-tree-slp-vectorize -fno-tree-vectorize -g -MMD -I$(KERNEL_DIR) $(XHCI_INCLUDE) -I$(DOOM_DIR) -DUSE_USB=$(USE_USB) -DDISABLE_PS2=$(DISABLE_PS2)
CPPFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti -std=c++17

ASFLAGS := --64
LDFLAGS := -nostdlib

.PHONY: all clean run

all: $(BOOT_EFI) $(KERNEL_BIN)

# Include dependencies
-include $(KERNEL_OBJS:.o=.d)
-include $(BOOT_OBJS:.o=.d)

# Bootloader build (REAL UEFI PE32+)
$(BOOT_EFI): $(BOOT_OBJS)
	@mkdir -p $(dir $@)
	$(UEFI_CC) $(BOOT_CFLAGS) \
		-Wl,-entry,efi_main \
		-Wl,--subsystem,10 \
		-nostdlib \
		-o $@ $^

# Kernel build
$(KERNEL_BIN): $(KERNEL_OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T $(KERNEL_LDS) -o $@ $^

$(BUILD_DIR)/boot/c/%.o: $(BOOT_DIR)/c/%.c
	@mkdir -p $(dir $@)
	$(UEFI_CC) $(BOOT_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/c/%.o: $(KERNEL_DIR)/c/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/doom/%.o: $(DOOM_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/cpp/%.o: $(KERNEL_DIR)/cpp/%.cpp
	@mkdir -p $(dir $@)
	g++ $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/asm/%.o: $(KERNEL_DIR)/asm/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run: all
	bash ./run.sh