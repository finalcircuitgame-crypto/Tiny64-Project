#include "../../include/efi.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

#define PT_LOAD 1
static Framebuffer g_fb;

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_STATUS status;

    ConOut->OutputString(ConOut, (CHAR16 *)L"Tiny64 C-Loader: Initializing...\r\n");

    // 1. GOP Setup
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    status = BS->LocateProtocol(&gopGuid, NULL, (void **)&gop);
    if (EFI_ERROR(status)) return status;

    g_fb.base_address = gop->Mode->FrameBufferBase;
    g_fb.buffer_size = gop->Mode->FrameBufferSize;
    g_fb.width = gop->Mode->Info->HorizontalResolution;
    g_fb.height = gop->Mode->Info->VerticalResolution;
    g_fb.pitch = gop->Mode->Info->PixelsPerScanLine * 4;

    ConOut->OutputString(ConOut, (CHAR16 *)L"GOP Initialized. Checking Boot Mode...\r\n");

    // 2. Check for RECOVERY file
    EFI_GUID sfspGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfsp;
    BS->LocateProtocol(&sfspGuid, NULL, (void **)&sfsp);
    
    EFI_FILE_PROTOCOL *root, *kernelFile, *recoveryFile;
    sfsp->OpenVolume(sfsp, &root);

    UINTN boot_flags = 0;
    status = root->Open(root, &recoveryFile, (CHAR16 *)L"\\RECOVERY", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(status)) {
        ConOut->OutputString(ConOut, (CHAR16 *)L"RECOVERY file detected. Booting in Recovery Mode...\r\n");
        boot_flags |= 1;
        recoveryFile->Close(recoveryFile);
    }

    // 3. Load Kernel.elf
    status = root->Open(root, &kernelFile, (CHAR16 *)L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        ConOut->OutputString(ConOut, (CHAR16 *)L"Error: kernel.elf not found!\r\n");
        return status;
    }

    // Read ELF Header
    Elf64_Ehdr elf_header;
    UINTN readSize = sizeof(Elf64_Ehdr);
    kernelFile->Read(kernelFile, &readSize, &elf_header);

    // Read Program Headers
    kernelFile->SetPosition(kernelFile, elf_header.e_phoff);
    UINTN phdr_size = elf_header.e_phnum * elf_header.e_phentsize;
    Elf64_Phdr *phdrs;
    BS->AllocatePool(EfiLoaderData, phdr_size, (void **)&phdrs);
    kernelFile->Read(kernelFile, &phdr_size, phdrs);

    // Calculate Memory Range
    UINT64 min_vaddr = 0xFFFFFFFFFFFFFFFF;
    UINT64 max_vaddr = 0;
    for (UINTN i = 0; i < elf_header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (phdrs[i].p_vaddr < min_vaddr) min_vaddr = phdrs[i].p_vaddr;
            if (phdrs[i].p_vaddr + phdrs[i].p_memsz > max_vaddr) 
                max_vaddr = phdrs[i].p_vaddr + phdrs[i].p_memsz;
        }
    }

    // Allocate Pages at specific address
    EFI_PHYSICAL_ADDRESS kernel_phys_base = min_vaddr; 
    UINTN pages = (max_vaddr - min_vaddr + 0xFFF) / 0x1000;
    
    status = BS->AllocatePages(AllocateAddress, EfiLoaderCode, pages, &kernel_phys_base);
    if (EFI_ERROR(status)) {
        ConOut->OutputString(ConOut, (CHAR16 *)L"Warning: 1MB mark blocked. Falling back...\r\n");
        status = BS->AllocatePages(AllocateAnyPages, EfiLoaderCode, pages, &kernel_phys_base);
        if (EFI_ERROR(status)) return status;
    }

    // Load Segments into Memory
    for (UINTN i = 0; i < elf_header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            void *dest = (void *)(kernel_phys_base + (phdrs[i].p_vaddr - min_vaddr));
            kernelFile->SetPosition(kernelFile, phdrs[i].p_offset);
            UINTN size = (UINTN)phdrs[i].p_filesz;
            kernelFile->Read(kernelFile, &size, dest);
            
            // Clear BSS
            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                memset((uint8_t*)dest + phdrs[i].p_filesz, 0, (size_t)(phdrs[i].p_memsz - phdrs[i].p_filesz));
            }
        }
    }

    kernelFile->Close(kernelFile);
    ConOut->OutputString(ConOut, (CHAR16 *)L"Kernel loaded. Exiting Boot Services...\r\n");

    // 3. Exit Boot Services
    UINTN mSize = 0, mKey = 0, dSize = 0;
    UINT32 dVer = 0;
    void *mMap = NULL;

    BS->GetMemoryMap(&mSize, NULL, &mKey, &dSize, &dVer);
    mSize += 4096; // Add generous padding
    BS->AllocatePool(EfiLoaderData, mSize, &mMap);
    
    // Get map and try to exit
    BS->GetMemoryMap(&mSize, mMap, &mKey, &dSize, &dVer);
    while (EFI_ERROR(BS->ExitBootServices(ImageHandle, mKey))) {
        // Retry: Update map key and try again
        BS->GetMemoryMap(&mSize, mMap, &mKey, &dSize, &dVer);
    }

    // 4. Jump to Kernel
    uint64_t entry_phys = kernel_phys_base + (elf_header.e_entry - min_vaddr);
    
    // ABI: MS x64 (UEFI) -> SysV (Kernel)
    // Args: RCX, RDX, R8, R9, R10 -> RDI, RSI, RDX, RCX, R8
    typedef void (*kernel_entry_t)(Framebuffer *, void*, UINTN, UINTN, UINTN);
    kernel_entry_t kernel_entry = (kernel_entry_t)entry_phys;
    
    kernel_entry(&g_fb, mMap, mSize, dSize, boot_flags);

    while(1) {
        __asm__ volatile ("hlt");
    }
    return EFI_SUCCESS;
}
