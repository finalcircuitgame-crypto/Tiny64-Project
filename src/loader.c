#include "common.h"
#include "serial.h"
#include <stdint.h>

typedef uint64_t size_t;
typedef uint64_t uintn_t;
typedef int64_t intn_t;

typedef void *EFI_HANDLE;
typedef uintn_t EFI_STATUS;

typedef struct {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
} EFI_GUID;

typedef struct {
  uint64_t Signature;
  uint32_t Revision;
  uint32_t HeaderSize;
  uint32_t CRC32;
  uint32_t Reserved;
} EFI_TABLE_HEADER;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (*EFI_TEXT_RESET)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    uint8_t ExtendedVerification);
typedef EFI_STATUS (*EFI_TEXT_STRING)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, uint16_t *String);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  EFI_TEXT_RESET Reset;
  EFI_TEXT_STRING OutputString;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// GOP Protocols
typedef enum {
  PixelRedGreenBlueReserved8BitPerColor,
  PixelBlueGreenRedReserved8BitPerColor,
  PixelBitMask,
  PixelBltOnly,
  PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
  uint32_t RedMask;
  uint32_t GreenMask;
  uint32_t BlueMask;
  uint32_t ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
  uint32_t Version;
  uint32_t HorizontalResolution;
  uint32_t VerticalResolution;
  EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
  EFI_PIXEL_BITMASK PixelInformation;
  uint32_t PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
  uint32_t MaxMode;
  uint32_t Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  uintn_t SizeOfInfo;
  uint64_t FrameBufferBase;
  uintn_t FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
  void *QueryMode;
  void *SetMode;
  void *Blt;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

// File Protocols
struct _EFI_FILE_PROTOCOL;
typedef EFI_STATUS (*EFI_FILE_OPEN)(struct _EFI_FILE_PROTOCOL *This,
                                    struct _EFI_FILE_PROTOCOL **NewHandle,
                                    uint16_t *FileName, uint64_t OpenMode,
                                    uint64_t Attributes);
typedef EFI_STATUS (*EFI_FILE_CLOSE)(struct _EFI_FILE_PROTOCOL *This);
typedef EFI_STATUS (*EFI_FILE_READ)(struct _EFI_FILE_PROTOCOL *This,
                                    uintn_t *BufferSize, void *Buffer);

typedef struct _EFI_FILE_PROTOCOL {
  uint64_t Revision;
  EFI_FILE_OPEN Open;
  EFI_FILE_CLOSE Close;
  void *Delete;
  EFI_FILE_READ Read;
} EFI_FILE_PROTOCOL;

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
  uint64_t Revision;
  EFI_STATUS (*OpenVolume)(struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
                           EFI_FILE_PROTOCOL **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct _EFI_LOADED_IMAGE_PROTOCOL {
  uint32_t Revision;
  uint32_t Reserved;
  EFI_HANDLE ParentHandle;
  struct _EFI_SYSTEM_TABLE *SystemTable;
  EFI_HANDLE DeviceHandle;
} EFI_LOADED_IMAGE_PROTOCOL;

// System Table & Boot Services
typedef struct {
  EFI_TABLE_HEADER Hdr;
  void *RaiseTPL;
  void *RestoreTPL;
  EFI_STATUS (*AllocatePages)(int Type, int MemoryType, uintn_t Pages,
                              uint64_t *Memory);
  EFI_STATUS (*FreePages)(uint64_t Memory, uintn_t Pages);
  EFI_STATUS (*GetMemoryMap)(uintn_t *MemoryMapSize, void *MemoryMap,
                             uintn_t *MapKey, uintn_t *DescriptorSize,
                             uint32_t *DescriptorVersion);
  EFI_STATUS (*AllocatePool)(int PoolType, uintn_t Size, void **Buffer);
  EFI_STATUS (*FreePool)(void *Buffer);
  void *CreateEvent;
  void *SetTimer;
  void *WaitForEvent;
  void *SignalEvent;
  void *CloseEvent;
  void *CheckEvent;
  void *InstallProtocolInterface;
  void *ReinstallProtocolInterface;
  void *UninstallProtocolInterface;
  EFI_STATUS (*HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                               void **Interface);
  void *PCHandleProtocol;
  void *RegisterProtocolNotify;
  void *LocateHandle;
  void *LocateDevicePath;
  void *InstallConfigurationTable;
  void *LoadImage;
  void *StartImage;
  void *Exit;
  void *UnloadImage;
  EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, uintn_t MapKey);
  void *GetNextMonotonicCount;
  void *Stall;
  void *SetWatchdogTimer;
  void *ConnectController;
  void *DisconnectController;
  void *OpenProtocol;
  void *CloseProtocol;
  void *OpenProtocolInformation;
  void *ProtocolsPerHandle;
  void *LocateHandleBuffer;
  EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, void *Registration,
                               void **Interface);
} EFI_BOOT_SERVICES;

typedef struct _EFI_SYSTEM_TABLE {
  EFI_TABLE_HEADER Hdr;
  uint16_t *FirmwareVendor;
  uint32_t FirmwareRevision;
  EFI_HANDLE ConsoleInHandle;
  void *ConIn;
  EFI_HANDLE ConsoleOutHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
  EFI_HANDLE StandardErrorHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
  void *RuntimeServices;
  EFI_BOOT_SERVICES *BootServices;
  uintn_t NumberOfTableEntries;
  void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

static EFI_GUID gop_guid = {0x9042a9de,
                            0x23dc,
                            0x4a38,
                            {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
static EFI_GUID sfs_guid = {0x964e5b22,
                            0x6459,
                            0x11d2,
                            {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
static EFI_GUID li_guid = {0x5b1b31a1,
                           0x9562,
                           0x11d2,
                           {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  serial_init();
  serial_println("--- Tiny64 UEFI Loader Started ---");

  // 1. Locate GOP
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  EFI_STATUS status =
      SystemTable->BootServices->LocateProtocol(&gop_guid, 0, (void **)&gop);
  if (status != 0) {
    serial_print("Error: Locate GOP failed. Status: ");
    serial_print_hex(status);
    serial_println("");
    while (1)
      ;
  }
  serial_println("GOP Located.");

  // 2. Locate SFS on the boot device
  serial_println("Opening LoadedImage protocol...");
  EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
  status = SystemTable->BootServices->HandleProtocol(ImageHandle, &li_guid,
                                                     (void **)&loaded_image);
  if (status != 0) {
    serial_print("Error: HandleProtocol(li) failed. Status: ");
    serial_print_hex(status);
    serial_println("");
    while (1)
      ;
  }

  serial_println("Locating SFS on device handle...");
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs;
  status = SystemTable->BootServices->HandleProtocol(loaded_image->DeviceHandle,
                                                     &sfs_guid, (void **)&sfs);

  if (status != 0) {
    serial_println("Attempting global LocateProtocol for SFS...");
    status =
        SystemTable->BootServices->LocateProtocol(&sfs_guid, 0, (void **)&sfs);
  }

  if (status != 0) {
    serial_print("Error: Could not find SFS anywhere. Status: ");
    serial_print_hex(status);
    serial_println("");
    while (1)
      ;
  }
  serial_println("SFS Located.");

  serial_println("Opening Volume...");
  EFI_FILE_PROTOCOL *root;
  status = sfs->OpenVolume(sfs, &root);
  if (status != 0) {
    serial_print("Error: OpenVolume failed. Status: ");
    serial_print_hex(status);
    serial_println("");
    while (1)
      ;
  }

  serial_println("Opening kernel.bin...");
  EFI_FILE_PROTOCOL *kernel_file;
  status = root->Open(root, &kernel_file, L"kernel.bin", 1, 0);
  if (status != 0) {
    serial_print("Error: Open(kernel.bin) failed. Status: ");
    serial_print_hex(status);
    serial_println("");
    while (1)
      ;
  }

  serial_println("Allocating pages for kernel...");
  uint64_t kernel_buffer_addr;
  status =
      SystemTable->BootServices->AllocatePages(0, 2, 16, &kernel_buffer_addr);
  if (status != 0) {
    serial_println("Error: AllocatePages failed.");
    while (1)
      ;
  }
  void *kernel_buffer = (void *)kernel_buffer_addr;

  serial_println("Reading kernel file...");
  uintn_t kernel_size = 65536;
  kernel_file->Read(kernel_file, &kernel_size, kernel_buffer);
  serial_println("Kernel binary loaded.");

  // 3. Prepare Boot Info
  BootInfo boot_info;
  boot_info.fb.FramebufferBase = gop->Mode->FrameBufferBase;
  boot_info.fb.FramebufferSize = gop->Mode->FrameBufferSize;
  boot_info.fb.HorizontalResolution = gop->Mode->Info->HorizontalResolution;
  boot_info.fb.VerticalResolution = gop->Mode->Info->VerticalResolution;
  boot_info.fb.PixelsPerScanLine = gop->Mode->Info->PixelsPerScanLine;

  // 4. Exit Boot Services
  serial_println("Exiting Boot Services...");
  uintn_t map_size = 0;
  uintn_t map_key = 0;
  uintn_t desc_size = 0;
  uint32_t desc_ver = 0;
  SystemTable->BootServices->GetMemoryMap(&map_size, 0, &map_key, &desc_size,
                                          &desc_ver);
  map_size += 2 * desc_size; // Add some extra space for descriptor overhead

  void *mmap;
  SystemTable->BootServices->AllocatePool(2, map_size, &mmap);
  SystemTable->BootServices->GetMemoryMap(&map_size, mmap, &map_key, &desc_size,
                                          &desc_ver);

  boot_info.mmap_addr = (uint64_t)mmap;
  boot_info.mmap_size = map_size;
  boot_info.mmap_desc_size = desc_size;

  status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
  if (status != 0) {
    serial_println("ExitBootServices failed (MapKey mismatch?), retrying...");
    SystemTable->BootServices->GetMemoryMap(&map_size, mmap, &map_key,
                                            &desc_size, &desc_ver);
    status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
  }

  if (status != 0) {
    serial_print("Error: ExitBootServices failed permanently. Status: ");
    serial_print_hex(status);
    serial_println("");
    while (1)
      ;
  }

  serial_println("Jump to kernel entry point.");

  // 5. Jump to Kernel
  typedef void (*kernel_entry_t)(BootInfo *);
  kernel_entry_t kernel_entry = (kernel_entry_t)kernel_buffer;
  kernel_entry(&boot_info);

  while (1)
    ;
  return 0;
}
