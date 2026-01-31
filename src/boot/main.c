#include <stdint.h>
#include "../../include/common.h"

// Basic UEFI types
typedef unsigned long long size_t;
typedef uint64_t UINTN;
typedef int64_t INTN;
typedef void* EFI_HANDLE;
typedef UINTN EFI_STATUS;

#define EFI_SUCCESS 0
#define EFI_LOAD_ERROR 1

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (*EFI_TEXT_STRING)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, uint16_t *String);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void* Reset;
    EFI_TEXT_STRING OutputString;
    // ... other fields omitted
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    uint16_t *FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    // ... other fields omitted
} EFI_SYSTEM_TABLE;

// Simple kernel entry point type
typedef void (*KernelEntry)(BootInfo*);

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (uint16_t*)L"Tiny64 UEFI Bootloader Starting...\r\n");

    // In a real OS, we would:
    // 1. Load the kernel file from the disk
    // 2. Set up GOP (Graphics Output Protocol)
    // 3. Get Memory Map
    // 4. Exit Boot Services
    // 5. Jump to Kernel

    // For this "simple" demo, we'll simulate the jump if we had a kernel.
    // Since we are creating a kernel in the same project, we'll link it or load it.
    
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (uint16_t*)L"Exiting to Kernel (Simulated)...
\n");

    return EFI_SUCCESS;
}
