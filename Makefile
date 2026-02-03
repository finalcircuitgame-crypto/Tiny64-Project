# Tiny64 – Advanced UEFI x86_64 OS Makefile

# Tools
EFI_CC  ?= x86_64-w64-mingw32-gcc
CC      ?= gcc
AS      ?= as
LD      ?= ld
OBJCOPY ?= objcopy

# OVMF paths
OVMF_CODE ?= $(firstword $(wildcard OVMF_CODE*.fd /usr/share/OVMF/OVMF_CODE*.fd /usr/share/ovmf/OVMF_CODE*.fd))
OVMF_VARS ?= $(firstword $(wildcard OVMF_VARS*.fd /usr/share/OVMF/OVMF_VARS*.fd /usr/share/ovmf/OVMF_VARS*.fd))

ifeq ($(OVMF_CODE),)
$(warning OVMF_CODE.fd not found; install ovmf or place file in project root.)
OVMF_CODE := OVMF_CODE.fd
endif

ifeq ($(OVMF_VARS),)
$(warning OVMF_VARS.fd not found; install ovmf or place file in project root.)
OVMF_VARS := OVMF_VARS.fd
endif

# Flags
CFLAGS     := -O2 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -g \
              -Iinclude -Iarch/x86/include
EFI_CFLAGS := -O2 -Iboot/efi -Iinclude -ffreestanding -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables \
              -mno-red-zone -mabi=ms -DEFI_FUNCTION_WRAPPER -fshort-wchar
LDFLAGS    := -nostdlib -z max-page-size=0x1000

# Directories
BUILD    := build
ARCH     := x86
ARCH_DIR := arch/$(ARCH)

# Targets
KERNEL_ELF := $(BUILD)/kernel.elf
EFI_BIN    := $(BUILD)/BOOTX64.EFI
DISK_IMG   := $(BUILD)/disk.img
OVMF_VARS_COPY := $(BUILD)/OVMF_VARS.fd

# Source files
KERNEL_SRC := $(wildcard kernel/*.c) \
              $(wildcard lib/*.c) \
              $(wildcard drivers/*.c) \
              $(wildcard $(ARCH_DIR)/kernel/*.c)
KERNEL_ASM := $(wildcard $(ARCH_DIR)/boot/*.S) \
              $(wildcard $(ARCH_DIR)/kernel/*.S)
KERNEL_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_SRC)) \
              $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_ASM))

EFI_SRC := $(wildcard boot/efi/*.c)
EFI_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(EFI_SRC))

.PHONY: all clean run image

all: $(EFI_BIN) $(KERNEL_ELF)

# Kernel objects
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJ) $(ARCH_DIR)/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T $(ARCH_DIR)/linker.ld -o $@ $(KERNEL_OBJ)

# EFI objects
$(BUILD)/boot/efi/%.o: boot/efi/%.c
	@mkdir -p $(dir $@)
	$(EFI_CC) $(EFI_CFLAGS) -c $< -o $@

$(EFI_BIN): $(EFI_OBJ)
	@mkdir -p $(dir $@)
	$(EFI_CC) $(EFI_CFLAGS) -Wl,-entry,efi_main -Wl,--subsystem,10 -shared -nostdlib -o $@ $^

image: all
	@echo "[IMG] Creating UEFI disk image..."
	@mkdir -p $(BUILD)
	@rm -f $(DISK_IMG)
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	@echo "[GPT] Partitioning..."
	@sgdisk -o $(DISK_IMG)
	@sgdisk -n 1:2048: -t 1:ef00 $(DISK_IMG)
	@echo "[FAT] Formatting ESP and copying files..."
	@mformat -i $(DISK_IMG)@@1M -F -v "TINY64_ESP" ::
	@mmd -i $(DISK_IMG)@@1M ::/EFI
	@mmd -i $(DISK_IMG)@@1M ::/EFI/BOOT
	@mcopy -i $(DISK_IMG)@@1M $(EFI_BIN) ::/EFI/BOOT/BOOTX64.EFI
	@mcopy -i $(DISK_IMG)@@1M $(KERNEL_ELF) ::/kernel.elf
	@echo "[OK] disk.img ready."

run: image
	@if [ ! -f $(OVMF_VARS_COPY) ]; then cp $(OVMF_VARS) $(OVMF_VARS_COPY); chmod +w $(OVMF_VARS_COPY); fi
	$(eval WIN_HOST := $(shell grep nameserver /etc/resolv.conf 2>/dev/null | head -1 | awk '{print $$2}'))
	$(if $(WIN_HOST),,$(eval WIN_HOST := 127.0.0.1))
	@echo "Sending packets to Windows host: $(WIN_HOST):60000"
	qemu-system-x86_64 -m 256M \
	-drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
	-drive if=pflash,format=raw,unit=1,file=$(OVMF_VARS_COPY) \
	-drive file=$(DISK_IMG),format=raw,if=ide \
	-serial stdio \
	-netdev socket,id=n0,udp=$(WIN_HOST):60000,localaddr=0.0.0.0:60001 \
	-device e1000,netdev=n0


clean:
	rm -rf $(BUILD)