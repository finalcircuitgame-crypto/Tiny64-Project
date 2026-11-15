CC = gcc
LD = ld
OBJCOPY = objcopy

SRCDIR = src
BOOTDIR = $(SRCDIR)/boot
KERNELDIR = $(SRCDIR)/kernel
DISTDIR = dist
OBJDIR = $(DISTDIR)/obj

EFI_INC = /usr/include/efi
EFI_LIB = /usr/lib
CFLAGS_EFI = -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(SRCDIR)/include -DEFI_FUNCTION_WRAPPER -DGNU_EFI_USE_MS_ABI
LDFLAGS_EFI = -nostdlib -znocombreloc -T $(EFI_LIB)/elf_x86_64_efi.lds -shared -Bsymbolic -L$(EFI_LIB) $(EFI_LIB)/crt0-efi-x86_64.o

CFLAGS_KERNEL = -ffreestanding -mno-red-zone -mcmodel=large -fno-pie -I$(SRCDIR)/include
LDFLAGS_KERNEL = -nostdlib -T $(KERNELDIR)/linker.ld -z max-page-size=0x1000

BOOTLOADER_EFI = $(DISTDIR)/BOOTX64.EFI
KERNEL_ELF = $(DISTDIR)/kernel.elf
DISK_IMG = $(DISTDIR)/tiny64.img

# The framebuffer console reads the embedded 8x8 CGA font.
# objcopy exports _binary_CGA_F08_start/_end for console.c.
FONT_OBJ = $(OBJDIR)/font.o

# Example: collect kernel sources/objects (adjust to your tree)
KERNEL_SRCS := $(shell find $(KERNELDIR) -name "*.c")
KERNEL_ASMS := $(shell find $(KERNELDIR) -name "*.s")
KERNEL_OBJS := $(patsubst $(KERNELDIR)/%.c,$(OBJDIR)/%.o,$(KERNEL_SRCS)) \
               $(patsubst $(KERNELDIR)/%.s,$(OBJDIR)/%_asm.o,$(KERNEL_ASMS))

.PHONY: all clean

all: setup $(DISK_IMG)

setup:
	mkdir -p $(OBJDIR)

$(BOOTLOADER_EFI): $(BOOTDIR)/main.c
	@mkdir -p $(dir $(OBJDIR)/boot_main.o)
	$(CC) $(CFLAGS_EFI) -c $< -o $(OBJDIR)/boot_main.o
	$(LD) $(LDFLAGS_EFI) $(OBJDIR)/boot_main.o -o $(OBJDIR)/boot_main.so -lgnuefi -lefi
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $(OBJDIR)/boot_main.so $@

# Turn the raw CGA font into an ELF object so its binary symbols are available
# to the framebuffer console at link time.
$(FONT_OBJ): CGA.F08
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386 $< $@

$(KERNEL_ELF): $(KERNEL_OBJS) $(FONT_OBJ)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS_KERNEL) $(KERNEL_OBJS) $(FONT_OBJ) -o $@

$(OBJDIR)/%.o: $(KERNELDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

$(OBJDIR)/%_asm.o: $(KERNELDIR)/%.s
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

$(DISK_IMG): $(BOOTLOADER_EFI) $(KERNEL_ELF)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	mformat -i $(DISK_IMG) -F ::
	mmd -i $(DISK_IMG) ::/EFI
	mmd -i $(DISK_IMG) ::/EFI/BOOT
	mcopy -o -i $(DISK_IMG) $(BOOTLOADER_EFI) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -o -i $(DISK_IMG) $(KERNEL_ELF) ::/kernel.elf

clean:
	rm -rf $(DISTDIR)/*
