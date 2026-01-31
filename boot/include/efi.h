#pragma once
#include <stdint.h>
#include <wchar.h>
#include <stdbool.h>

typedef uint64_t EFI_STATUS;
typedef void*    EFI_HANDLE;
typedef wchar_t  CHAR16;
typedef uint64_t UINTN;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint8_t  BOOLEAN;
typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int16_t  INT16;

#define EFI_SUCCESS 0
#define EFIAPI __attribute__((ms_abi))
#define EFI_ERROR(a) ((a) != EFI_SUCCESS)
#define EFI_LOAD_ERROR            0x8000000000000001ULL
#define EFI_INVALID_PARAMETER     0x8000000000000002ULL
#define EFI_UNSUPPORTED           0x8000000000000003ULL
#define EFI_BAD_BUFFER_SIZE       0x8000000000000004ULL
#define EFI_BUFFER_TOO_SMALL      0x8000000000000005ULL
#define EFI_NOT_READY             0x8000000000000006ULL
#define EFI_DEVICE_ERROR          0x8000000000000007ULL
#define EFI_WRITE_PROTECTED       0x8000000000000008ULL
#define EFI_OUT_OF_RESOURCES      0x8000000000000009ULL
#define EFI_VOLUME_CORRUPTED      0x800000000000000AULL
#define EFI_VOLUME_FULL           0x800000000000000BULL
#define EFI_NO_MEDIA              0x800000000000000CULL
#define EFI_MEDIA_CHANGED         0x800000000000000DULL
#define EFI_NOT_FOUND             0x800000000000000EULL
#define EFI_ACCESS_DENIED         0x800000000000000FULL
#define EFI_NO_RESPONSE           0x8000000000000010ULL
#define EFI_NO_MAPPING            0x8000000000000011ULL
#define EFI_TIMEOUT               0x8000000000000012ULL
#define EFI_NOT_STARTED           0x8000000000000013ULL
#define EFI_ALREADY_STARTED       0x8000000000000014ULL
#define EFI_ABORTED               0x8000000000000015ULL
#define EFI_ICMP_ERROR            0x8000000000000016ULL
#define EFI_TFTP_ERROR            0x8000000000000017ULL
#define EFI_PROTOCOL_ERROR        0x8000000000000018ULL


typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} EFI_GUID;

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (EFIAPI *TestString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (EFIAPI *QueryMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN*, UINTN*);
    EFI_STATUS (EFIAPI *SetMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (EFIAPI *SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (EFIAPI *ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*);
    EFI_STATUS (EFIAPI *SetCursorPosition)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN);
    EFI_STATUS (EFIAPI *EnableCursor)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
};

typedef struct {
    uint32_t MaxMode;
    uint32_t Mode;
    uint32_t Attribute;
    uint32_t CursorColumn;
    uint32_t CursorRow;
    BOOLEAN  Visible;
} EFI_SIMPLE_TEXT_OUTPUT_MODE;

typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;

    void* RaiseTPL;
    void* RestoreTPL;

    EFI_STATUS (EFIAPI *AllocatePages)(uint32_t, uint32_t, UINTN, uint64_t*);
    EFI_STATUS (EFIAPI *FreePages)(uint64_t, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN*, void*, UINTN*, UINTN*, uint32_t*);
    EFI_STATUS (EFIAPI *AllocatePool)(uint32_t, UINTN, void**);
    EFI_STATUS (EFIAPI *FreePool)(void*);

    void* CreateEvent;
    void* SetTimer;
    void* WaitForEvent;
    void* SignalEvent;
    void* CloseEvent;
    void* CheckEvent;

    void* InstallProtocolInterface;
    void* ReinstallProtocolInterface;
    void* UninstallProtocolInterface;
    void* HandleProtocol;
    void* Reserved;                // <<< add this
    void* RegisterProtocolNotify;
    void* LocateHandle;
    void* LocateDevicePath;
    void* InstallConfigurationTable;

    void* LoadImage;
    void* StartImage;
    void* Exit;
    void* UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);

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
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID*, void*, void**);
    void* InstallMultipleProtocolInterfaces;
    void* UninstallMultipleProtocolInterfaces;

    void* CalculateCrc32;

    void* CopyMem;
    void* SetMem;

    void* CreateEventEx;
};


typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16* FirmwareVendor;
    uint32_t FirmwareRevision;

    EFI_HANDLE ConsoleInHandle;
    void* ConIn;

    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut;

    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr;

    void* RuntimeServices;
    EFI_BOOT_SERVICES* BootServices;

    UINTN NumberOfTableEntries;
    void* ConfigurationTable;
} EFI_SYSTEM_TABLE;

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}}

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}}

typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;

struct EFI_FILE_PROTOCOL {
    UINTN Revision;
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL*, EFI_FILE_PROTOCOL**, CHAR16*, UINTN, UINTN);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Delete)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL*, UINTN*, void*);
    EFI_STATUS (EFIAPI *Write)(EFI_FILE_PROTOCOL*, UINTN*, void*);
    EFI_STATUS (EFIAPI *GetPosition)(EFI_FILE_PROTOCOL*, UINTN*);
    EFI_STATUS (EFIAPI *SetPosition)(EFI_FILE_PROTOCOL*, UINTN);
    EFI_STATUS (EFIAPI *GetInfo)(EFI_FILE_PROTOCOL*, EFI_GUID*, UINTN*, void*);
    EFI_STATUS (EFIAPI *SetInfo)(EFI_FILE_PROTOCOL*, EFI_GUID*, UINTN, void*);
    EFI_STATUS (EFIAPI *Flush)(EFI_FILE_PROTOCOL*);
};

typedef struct {
    UINTN Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(void*, EFI_FILE_PROTOCOL**);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    uint32_t Version;
    uint32_t HorizontalResolution;
    uint32_t VerticalResolution;
    struct {
        uint32_t RedMask;
        uint32_t GreenMask;
        uint32_t BlueMask;
        uint32_t ReservedMask;
    } PixelInformation;
    uint32_t PixelFormat;
    uint32_t PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    AllocateMax
} EFI_ALLOCATE_TYPE;


#define EFI_FILE_MODE_READ  0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE 0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

typedef struct {
    UINTN   Size;
    UINT64  FileSize;
    UINT64  PhysicalSize;
    EFI_TIME CreationTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64  Attribute;
    CHAR16  FileName[1];
} EFI_FILE_INFO;

#define EFI_FILE_INFO_GUID \
    {0x09576e92,0x6d3f,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}}

typedef struct {
    EFI_STATUS (EFIAPI *QueryMode)(void*, uint32_t, uint32_t*, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**);
    EFI_STATUS (EFIAPI *SetMode)(void*, uint32_t);
    EFI_STATUS (EFIAPI *Blt)(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    struct {
        uint32_t MaxMode;
        uint32_t Mode;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
        UINTN SizeOfInfo;
        UINTN FrameBufferBase;
        UINTN FrameBufferSize;
    } *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    uint64_t base_address;
    uint64_t buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} Framebuffer;

typedef struct {
    UINT32 Type;
    UINT32 Pad;
    UINTN  PhysicalStart;
    UINTN  VirtualStart;
    UINTN  NumberOfPages;
    uint64_t Attribute;
} MemoryMapEntry;
