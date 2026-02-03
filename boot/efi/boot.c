#include <stddef.h>
#include <stdint.h>
#include <kernel/bootinfo.h>
#include <kernel/efi_types.h>

// --- EFI Definitions ---
typedef uint64_t UINTN;
typedef int64_t  INTN;
typedef void*    EFI_HANDLE;
typedef uint8_t  BOOLEAN;
typedef uint16_t CHAR16;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;
typedef UINTN    EFI_STATUS;

#define EFI_SUCCESS 0

typedef struct { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; } EFI_GUID;

static EFI_GUID LoadedImageProtocol = {0x5B1B31A1,0x9562,0x11d2,{0x8E,0x3F,0x00,0xA0,0xC9,0x69,0x72,0x3B}}; 
static EFI_GUID SimpleFileSystemProtocol = {0x964E5B22,0x6459,0x11d2,{0x8E,0x39,0x00,0xA0,0xC9,0x69,0x72,0x3B}};
static EFI_GUID GraphicsOutputProtocol = {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (*EFI_TEXT_STRING)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const CHAR16 *String);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void* Reset;
    EFI_TEXT_STRING OutputString;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef enum { AllocateAnyPages, AllocateMaxAddress, AllocateAddress, MaxAllocateType } EFI_ALLOCATE_TYPE;


typedef struct _EFI_FILE_PROTOCOL {
    uint64_t Revision;
    EFI_STATUS (*Open)(struct _EFI_FILE_PROTOCOL *This, struct _EFI_FILE_PROTOCOL **NewHandle, const CHAR16 *FileName, uint64_t OpenMode, uint64_t Attributes);
    EFI_STATUS (*Close)(struct _EFI_FILE_PROTOCOL *This);
    void* Delete;
    EFI_STATUS (*Read)(struct _EFI_FILE_PROTOCOL *This, UINTN *BufferSize, void *Buffer);
} EFI_FILE_PROTOCOL;

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t Revision;
    EFI_STATUS (*OpenVolume)(struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, EFI_FILE_PROTOCOL **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    uint32_t Version;
    uint32_t HorizontalResolution;
    uint32_t VerticalResolution;
    uint32_t PixelFormat;
    uint32_t PixelInformation[4];
    uint32_t PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t MaxMode;
    uint32_t Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    void *QueryMode;
    void *SetMode;
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void* RaiseTPL;
    void* RestoreTPL;
    EFI_STATUS (*AllocatePages)(EFI_ALLOCATE_TYPE Type, EFI_MEMORY_TYPE MemoryType, UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory);
    EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS Memory, UINTN Pages);
    EFI_STATUS (*GetMemoryMap)(UINTN *MemoryMapSize, EFI_MEMORY_DESCRIPTOR *MemoryMap, UINTN *MapKey, UINTN *DescriptorSize, uint32_t *DescriptorVersion);
    void* AllocatePool;
    void* FreePool;
    void* CreateEvent;
    void* SetTimer;
    void* WaitForEvent;
    void* SignalEvent;
    void* CloseEvent;
    void* CheckEvent;
    void* InstallProtocolInterface;
    void* ReinstallProtocolInterface;
    void* UninstallProtocolInterface;
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, void **Interface);
    void* Reserved;
    void* RegisterProtocolNotify;
    void* LocateHandle;
    void* LocateDevicePath;
    void* InstallConfigurationTable;
    void* LoadImage;
    void* StartImage;
    void* Exit;
    void* UnloadImage;
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void* GetNextMonotonicCount;
    void* Stall;
    void* SetWatchdogTimer;
    void* ConnectController;
    void* DisconnectController;
    void* OpenProtocol;
    void* CloseProtocol;
    void* OpenProtocolInformation;
    void* ProtocolsPerHandle;
    void* LocateHandleBuffer;
    EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void* RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

typedef struct {
    uint32_t Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
} EFI_LOADED_IMAGE_PROTOCOL;

// --- ELF Definitions ---
#define EI_NIDENT 16
typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

#define PT_LOAD 1

// --- Helper Functions ---
void print(EFI_SYSTEM_TABLE *ST, const CHAR16 *s) {
    ST->ConOut->OutputString(ST->ConOut, (CHAR16*)s);
}

void print_hex(EFI_SYSTEM_TABLE *ST, uint64_t n) {
    CHAR16 buf[19];
    const CHAR16 *digits = L"0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buf[i + 2] = digits[n & 0xF];
        n >>= 4;
    }
    buf[18] = '\0';
    print(ST, buf);
}

static BootInfo boot_info;

void efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *root, *kernel_file;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    print(SystemTable, L"Tiny64 UEFI Loader Starting...\r\n");

    status = SystemTable->BootServices->HandleProtocol(ImageHandle, &LoadedImageProtocol, (void**)&loaded_image);
    if (status != EFI_SUCCESS) {
        print(SystemTable, L"Failed to get LoadedImage protocol. Status: ");
        print_hex(SystemTable, status);
        print(SystemTable, L"\r\n");
        return;
    }

    status = SystemTable->BootServices->HandleProtocol(loaded_image->DeviceHandle, &SimpleFileSystemProtocol, (void**)&fs);
    if (status != EFI_SUCCESS) {
        print(SystemTable, L"Failed to get FileSystem protocol. Status: ");
        print_hex(SystemTable, status);
        print(SystemTable, L"\r\n");
        return;
    }

    status = SystemTable->BootServices->LocateProtocol(&GraphicsOutputProtocol, NULL, (void**)&gop);
    if (status == EFI_SUCCESS) {
        boot_info.fb_base = (void*)gop->Mode->FrameBufferBase;
        boot_info.fb_size = gop->Mode->FrameBufferSize;
        boot_info.fb_width = gop->Mode->Info->HorizontalResolution;
        boot_info.fb_height = gop->Mode->Info->VerticalResolution;
        boot_info.fb_pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
    }

    status = fs->OpenVolume(fs, &root);
    if (status != EFI_SUCCESS) { print(SystemTable, L"Failed to open volume\r\n"); return; }

    status = root->Open(root, &kernel_file, (CHAR16*)L"kernel.elf", 1, 0); 
    if (status != EFI_SUCCESS) { print(SystemTable, L"Failed to open kernel.elf\r\n"); return; }

    EFI_PHYSICAL_ADDRESS kernel_buffer_phys;
    status = SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 8192, &kernel_buffer_phys);
    if (status != EFI_SUCCESS) { print(SystemTable, L"Failed to allocate buffer\r\n"); return; }
    
    void* kernel_buffer = (void*)kernel_buffer_phys;
    UINTN kernel_size = 32*1024*1024;
    status = kernel_file->Read(kernel_file, &kernel_size, kernel_buffer);
    if (status != EFI_SUCCESS) { print(SystemTable, L"Failed to read kernel into buffer\r\n"); return; }

    Elf64_Ehdr *e = (Elf64_Ehdr*)kernel_buffer;
    if (e->e_ident[0] != 0x7F || e->e_ident[1] != 'E' || e->e_ident[2] != 'L' || e->e_ident[3] != 'F') {
        print(SystemTable, L"Invalid ELF magic\r\n"); return;
    }

    Elf64_Phdr *p = (Elf64_Phdr*)(kernel_buffer + e->e_phoff);

    for (int i = 0; i < e->e_phnum; i++, p++) {
        if (p->p_type == PT_LOAD) {
            UINTN pages = (p->p_memsz + 4095) / 4096;
            EFI_PHYSICAL_ADDRESS segment_addr = p->p_paddr;
            
            status = SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderCode, pages, &segment_addr);
            if (status != EFI_SUCCESS) {
                print(SystemTable, L"Failed to allocate segment at fixed address: ");
                print_hex(SystemTable, p->p_paddr);
                print(SystemTable, L"\r\n");
                return;
            }

            uint8_t *src = (uint8_t*)kernel_buffer + p->p_offset;
            uint8_t *dest = (uint8_t*)segment_addr;
            for (UINTN j = 0; j < p->p_filesz; j++) dest[j] = src[j];
            for (UINTN j = p->p_filesz; j < p->p_memsz; j++) dest[j] = 0;
        }
    }

    print(SystemTable, L"Kernel loaded. Exiting Boot Services...\r\n");

    UINTN map_size = 0, map_key, desc_size;
    uint32_t desc_ver;
    SystemTable->BootServices->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 4 * desc_size;
    EFI_PHYSICAL_ADDRESS mmap_phys;
    SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, (map_size + 4095) / 4096, &mmap_phys);
    SystemTable->BootServices->GetMemoryMap(&map_size, (EFI_MEMORY_DESCRIPTOR*)mmap_phys, &map_key, &desc_size, &desc_ver);

    boot_info.mmap = (void*)mmap_phys;
    boot_info.mmap_size = map_size;
    boot_info.mmap_desc_size = desc_size;

    status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        SystemTable->BootServices->GetMemoryMap(&map_size, (EFI_MEMORY_DESCRIPTOR*)mmap_phys, &map_key, &desc_size, &desc_ver);
        SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    }

    void (*kernel_entry)(BootInfo*) = (void (*)(BootInfo*))e->e_entry;
    kernel_entry(&boot_info);

    for(;;);
}