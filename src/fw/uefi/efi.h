/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Minimal UEFI type definitions (UEFI Specification 2.x, 64-bit). */
#ifndef BB_EFI_H
#define BB_EFI_H

#include <stdint.h>

/* UEFI on x86_64 uses the Microsoft x64 ABI. On AArch64 it is the standard
 * AAPCS64 and on IA32 plain cdecl, so no attribute is needed there. */
#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

/* UINTN and EFI_STATUS are as wide as a pointer (32 bits on IA32). */
#if __SIZEOF_POINTER__ == 8
typedef uint64_t UINTN;
#else
typedef uint32_t UINTN;
#endif
typedef UINTN EFI_STATUS;

/* Error codes have the top bit of EFI_STATUS set. */
#define EFI_ERROR_BIT ((UINTN)1 << (sizeof(UINTN) * 8 - 1))
#define EFI_ERR(n)    (EFI_ERROR_BIT | (UINTN)(n))
typedef void    *EFI_HANDLE;
typedef uint16_t CHAR16;
typedef uint8_t  BOOLEAN;

#define EFI_SUCCESS   ((UINTN)0)
/* High bit marks an error code in UEFI; NOT_READY is error number 6. */
#define EFI_NOT_READY EFI_ERR(6)
#define EFI_INVALID_PARAMETER EFI_ERR(2)
#define EFI_UNSUPPORTED EFI_ERR(3)
#define EFI_BUFFER_TOO_SMALL EFI_ERR(5)
#define EFI_LOAD_ERROR EFI_ERR(1)
#define EFI_NOT_FOUND EFI_ERR(14)

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    uint16_t ScanCode;
    CHAR16   UnicodeChar;
} EFI_INPUT_KEY;

struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(
    struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key);

typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    void              *Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    void              *WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* Scan codes for the keys BinBoot uses (UEFI spec, EFI_INPUT_KEY). */
#define EFI_SCAN_UP   0x01
#define EFI_SCAN_DOWN 0x02
#define EFI_SCAN_PAUSE 0x48     /* SCAN_PAUSE in the EDK II headers */

typedef struct {
    int32_t MaxMode;
    int32_t Mode;
    int32_t Attribute;
    int32_t CursorColumn;
    int32_t CursorRow;
    BOOLEAN CursorVisible;
} EFI_SIMPLE_TEXT_OUTPUT_MODE;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const CHAR16 *String);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_QUERY_MODE)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber,
    UINTN *Columns, UINTN *Rows);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_CURSOR_POSITION)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Column, UINTN Row);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Attribute);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_ENABLE_CURSOR)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN Visible);

/* Unused members stay void* so offsets of the used ones are correct. */
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void                        *Reset;
    EFI_TEXT_STRING              OutputString;
    void                        *TestString;
    EFI_TEXT_QUERY_MODE          QueryMode;
    void                        *SetMode;
    EFI_TEXT_SET_ATTRIBUTE       SetAttribute;
    EFI_TEXT_CLEAR_SCREEN        ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION SetCursorPosition;
    EFI_TEXT_ENABLE_CURSOR       EnableCursor;
    EFI_SIMPLE_TEXT_OUTPUT_MODE *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;


typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} EFI_GUID;

#define EFI_MEMORY_TYPE_LOADER_DATA 2
#define EFI_NATIVE_INTERFACE        0
#define EFI_FILE_MODE_READ          1ULL

typedef struct {
    uint8_t  Type;
    uint8_t  SubType;
    uint16_t Length;
} __attribute__((packed)) EFI_DEVICE_PATH_PROTOCOL;

/* Media / Hard Drive node: identifies a partition by GPT GUID or MBR id. */
typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;    /* Type 4, SubType 1 */
    uint32_t PartitionNumber;
    uint64_t PartitionStart;
    uint64_t PartitionSize;
    uint8_t  Signature[16];
    uint8_t  MBRType;
    uint8_t  SignatureType;             /* 1 = MBR (32-bit), 2 = GPT GUID */
} __attribute__((packed)) EFI_HARDDRIVE_DEVICE_PATH;

/* ---- EFI_LOADED_IMAGE_PROTOCOL ---- */
typedef struct {
    uint32_t Revision;
    EFI_HANDLE ParentHandle;
    void *SystemTable;
    EFI_HANDLE DeviceHandle;
    void *FilePath;
    void *Reserved;
    uint32_t LoadOptionsSize;
    void *LoadOptions;
    void *ImageBase;
    uint64_t ImageSize;
    int32_t ImageCodeType;
    int32_t ImageDataType;
    void *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

/* ---- EFI_SIMPLE_FILE_SYSTEM_PROTOCOL / EFI_FILE_PROTOCOL ---- */
struct EFI_FILE_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    struct EFI_FILE_PROTOCOL *This, struct EFI_FILE_PROTOCOL **NewHandle,
    const CHAR16 *FileName, uint64_t OpenMode, uint64_t Attributes);
typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(struct EFI_FILE_PROTOCOL *This);
typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    struct EFI_FILE_PROTOCOL *This, UINTN *BufferSize, void *Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    struct EFI_FILE_PROTOCOL *This, const EFI_GUID *InformationType,
    UINTN *BufferSize, void *Buffer);

typedef struct EFI_FILE_PROTOCOL {
    uint64_t          Revision;
    EFI_FILE_OPEN     Open;
    EFI_FILE_CLOSE    Close;
    void             *Delete;
    EFI_FILE_READ     Read;
    void             *Write;
    void             *GetPosition;
    void             *SetPosition;
    EFI_FILE_GET_INFO GetInfo;
    void             *SetInfo;
    void             *Flush;
} EFI_FILE_PROTOCOL;

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_OPEN_VOLUME)(
    struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, EFI_FILE_PROTOCOL **Root);
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t        Revision;
    EFI_OPEN_VOLUME OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    uint16_t Year;
    uint8_t  Month, Day, Hour, Minute, Second, Pad1;
    uint32_t Nanosecond;
    int16_t  TimeZone;
    uint8_t  Daylight, Pad2;
} EFI_TIME;

/* Only the fixed part is declared; FileName follows in memory. */
typedef struct {
    uint64_t Size;
    uint64_t FileSize;
    uint64_t PhysicalSize;
    EFI_TIME CreateTime, LastAccessTime, ModificationTime;
    uint64_t Attribute;
} EFI_FILE_INFO_HEADER;

/* ---- EFI_LOAD_FILE2_PROTOCOL (used to hand the initrd to Linux) ---- */
struct EFI_LOAD_FILE2_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_LOAD_FILE2)(
    struct EFI_LOAD_FILE2_PROTOCOL *This, void *FilePath,
    BOOLEAN BootPolicy, UINTN *BufferSize, void *Buffer);
typedef struct EFI_LOAD_FILE2_PROTOCOL {
    EFI_LOAD_FILE2 LoadFile;
} EFI_LOAD_FILE2_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_STALL)(UINTN Microseconds);

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(
    int32_t PoolType, UINTN Size, void **Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(void *Buffer);
typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    UINTN *MemoryMapSize, void *MemoryMap, UINTN *MapKey,
    UINTN *DescriptorSize, uint32_t *DescriptorVersion);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(
    const EFI_GUID *Protocol, void *Registration, void **Interface);

/* One entry of the firmware memory map (stride is DescriptorSize). */
typedef struct {
    uint32_t Type;
    uint32_t Pad;
    uint64_t PhysicalStart;
    uint64_t VirtualStart;
    uint64_t NumberOfPages;             /* 4 KiB pages */
    uint64_t Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* ---- EFI_GRAPHICS_OUTPUT_PROTOCOL (only what we read) ---- */
typedef struct {
    uint32_t Version;
    uint32_t HorizontalResolution;
    uint32_t VerticalResolution;
    int32_t  PixelFormat;
    uint32_t PixelInformation[4];
    uint32_t PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t MaxMode;
    uint32_t Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN    SizeOfInfo;
    uint64_t FrameBufferBase;
    UINTN    FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    void *QueryMode, *SetMode, *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

/* ---- EFI_PCI_IO_PROTOCOL (up to config-space access) ---- */
struct EFI_PCI_IO_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_PCI_IO_CONFIG)(
    struct EFI_PCI_IO_PROTOCOL *This, int32_t Width, uint32_t Offset,
    UINTN Count, void *Buffer);
#define EFI_PCI_IO_WIDTH_UINT32 2
typedef struct EFI_PCI_IO_PROTOCOL {
    void *PollMem, *PollIo;
    struct { void *Read, *Write; } Mem, Io;
    struct { EFI_PCI_IO_CONFIG Read, Write; } Pci;
} EFI_PCI_IO_PROTOCOL;
#define EFI_LOCATE_BY_PROTOCOL 2
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE)(
    int32_t SearchType, const EFI_GUID *Protocol, void *SearchKey,
    UINTN *BufferSize, EFI_HANDLE *Buffer);
typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(
    EFI_HANDLE Handle, const EFI_GUID *Protocol, void **Interface);
typedef EFI_STATUS (EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE *Handle, const EFI_GUID *Protocol, int32_t InterfaceType,
    void *Interface);
typedef EFI_STATUS (EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE Handle, const EFI_GUID *Protocol, void *Interface);

/* BootPolicy is FALSE for a plain load; SourceBuffer != NULL selects a
 * memory-to-memory load, in which case DevicePath may be NULL. */
typedef EFI_STATUS (EFIAPI *EFI_IMAGE_LOAD)(
    BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle, void *DevicePath,
    void *SourceBuffer, UINTN SourceSize, EFI_HANDLE *ImageHandle);
typedef EFI_STATUS (EFIAPI *EFI_IMAGE_START)(
    EFI_HANDLE ImageHandle, UINTN *ExitDataSize, CHAR16 **ExitData);

/* Member order follows the specification exactly. */
typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL, *RestoreTPL;
    void *AllocatePages, *FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;
    void *CreateEvent, *SetTimer, *WaitForEvent, *SignalEvent, *CloseEvent,
         *CheckEvent;
    EFI_INSTALL_PROTOCOL_INTERFACE InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    EFI_UNINSTALL_PROTOCOL_INTERFACE UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    void *Reserved, *RegisterProtocolNotify;
    EFI_LOCATE_HANDLE LocateHandle;
    void *LocateDevicePath, *InstallConfigurationTable;
    EFI_IMAGE_LOAD LoadImage;
    EFI_IMAGE_START StartImage;
    void *Exit, *UnloadImage, *ExitBootServices;
    void *GetNextMonotonicCount;
    EFI_STALL Stall;
    void *SetWatchdogTimer;
    void *ConnectController, *DisconnectController, *OpenProtocol,
         *CloseProtocol, *OpenProtocolInformation, *ProtocolsPerHandle,
         *LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

#endif /* BB_EFI_H */
