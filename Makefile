# Tools
CC = gcc
LD = ld
OBJCOPY = objcopy

# Directories
SRCDIR = src
BOOTDIR = $(SRCDIR)/boot
KERNELDIR = $(SRCDIR)/kernel
DISTDIR = dist
OBJDIR = $(DISTDIR)/obj

# Bootloader Flags
EFI_INC = /usr/include/efi
EFI_LIB = /usr/lib
CFLAGS_EFI = -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(SRCDIR)/include -DEFI_FUNCTION_WRAPPER -DGNU_EFI_USE_MS_ABI
LDFLAGS_EFI = -nostdlib -znocombreloc -T $(EFI_LIB)/elf_x86_64_efi.lds -shared -Bsymbolic -L$(EFI_LIB) $(EFI_LIB)/crt0-efi-x86_64.o

# Kernel Flags
CFLAGS_KERNEL = -ffreestanding -mno-red-zone -mcmodel=large -fno-pie -I$(SRCDIR)/include
LDFLAGS_KERNEL = -nostdlib -T $(KERNELDIR)/linker.ld -z max-page-size=0x1000

# Targets
BOOTLOADER_EFI = $(DISTDIR)/BOOTX64.EFI
KERNEL_ELF = $(DISTDIR)/kernel.elf
DISK_IMG = $(DISTDIR)/tiny64.img

# Kernel Objects
# Recursively find all C and S files
KERNEL_SRCS := $(shell find $(KERNELDIR) -name "*.c")
KERNEL_ASMS := $(shell find $(KERNELDIR) -name "*.s")

<<<<<<< HEAD
# Kernel sources
KERNEL_SRC_C_ALL := $(shell find $(KERNEL_DIR)/c -name '*.c')

# Only link ONE xHCI implementation.
# - USE_QEMU_XHCI=1: use kernel/c/drivers/usb/xhci/qemu/xhci.c
# - USE_QEMU_XHCI=0: use kernel/c/drivers/usb/xhci/xhci.c
ifeq ($(USE_QEMU_XHCI),1)
KERNEL_SRC_C := $(filter-out $(KERNEL_DIR)/c/drivers/usb/xhci/xhci.c,$(KERNEL_SRC_C_ALL))
else
KERNEL_SRC_C := $(filter-out $(KERNEL_DIR)/c/drivers/usb/xhci/qemu/xhci.c,$(KERNEL_SRC_C_ALL))
endif
XHCI_INCLUDE := -I$(KERNEL_DIR)/c/drivers/usb/xhci -I$(KERNEL_DIR)/c/drivers/usb/xhci/qemu
=======
# Map to object files in OBJDIR, maintaining directory structure
KERNEL_OBJS := $(patsubst $(KERNELDIR)/%.c, $(OBJDIR)/%.o, $(KERNEL_SRCS)) \
              $(patsubst $(KERNELDIR)/%.s, $(OBJDIR)/%_asm.o, $(KERNEL_ASMS))
FONT_OBJ = $(OBJDIR)/font.o
>>>>>>> 0b790d024abc49e24b6580cd08f06b247c2b72a1

all: setup $(DISK_IMG)

setup:
	mkdir -p $(OBJDIR)

# Build Bootloader
$(BOOTLOADER_EFI): $(BOOTDIR)/main.c
	@mkdir -p $(dir $(OBJDIR)/boot_main.o)
	$(CC) $(CFLAGS_EFI) -c $< -o $(OBJDIR)/boot_main.o
	$(LD) $(LDFLAGS_EFI) $(OBJDIR)/boot_main.o -o $(OBJDIR)/boot_main.so -lgnuefi -lefi
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $(OBJDIR)/boot_main.so $@

<<<<<<< HEAD
KERNEL_BIN := $(BUILD_DIR)/kernel/kernel.elf
KERNEL_LDS := $(KERNEL_DIR)/config/linker/kernel.ld

CFLAGS := -O3 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-red-zone -D_FORTIFY_SOURCE=0 -fno-tree-slp-vectorize -fno-tree-vectorize -g -MMD -I$(KERNEL_DIR) $(XHCI_INCLUDE) -I$(DOOM_DIR) -DUSE_USB=$(USE_USB) -DUSE_QEMU_XHCI=$(USE_QEMU_XHCI) -DDISABLE_PS2=$(DISABLE_PS2)
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
=======
# Embed Font
$(FONT_OBJ): CGA.F08
>>>>>>> 0b790d024abc49e24b6580cd08f06b247c2b72a1
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386 $< $@

# Build Kernel
$(KERNEL_ELF): $(KERNEL_OBJS) $(FONT_OBJ)
	$(LD) $(LDFLAGS_KERNEL) $(KERNEL_OBJS) $(FONT_OBJ) -o $@

# Pattern rules for nested kernel directories
$(OBJDIR)/%.o: $(KERNELDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

$(OBJDIR)/%_asm.o: $(KERNELDIR)/%.s
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

# Create Disk Image
$(DISK_IMG): $(BOOTLOADER_EFI) $(KERNEL_ELF)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	mformat -i $(DISK_IMG) -F ::
	mmd -i $(DISK_IMG) ::/EFI
	mmd -i $(DISK_IMG) ::/EFI/BOOT
	mcopy -o -i $(DISK_IMG) $(BOOTLOADER_EFI) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -o -i $(DISK_IMG) $(KERNEL_ELF) ::/kernel.elf

clean:
	rm -rf $(DISTDIR)/*