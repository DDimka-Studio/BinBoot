/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "config.h"
#include "uefi_info.h"
#include "info.h"            /* src/core/info.h */

#if BB_INFO_VIDEO
static const EFI_GUID guid_gop = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
static const EFI_GUID guid_pci_io = {
    0x4cf5b200, 0x68b8, 0x4ca5,
    {0x9e, 0xec, 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a}};
#endif

#if BB_INFO_MEMORY
/* Memory the OS can use: everything except reserved and device ranges. */
static int is_ram_type(uint32_t t)
{
    return (t >= 1 && t <= 7) || t == 9 || t == 10;
}

static void info_memory(EFI_SYSTEM_TABLE *st)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    UINTN size = 0, key, desc_size = 0, off;
    uint32_t desc_ver;
    uint8_t *map = 0;
    uint64_t pages = 0;
    unsigned mib;
    struct bb_sb v;
    EFI_STATUS status;

    bb_sb_init(&v);
    status = bs->GetMemoryMap(&size, 0, &key, &desc_size, &desc_ver);
    if (status == EFI_BUFFER_TOO_SMALL) {
        size += 8 * desc_size;              /* the map grows when we allocate */
        if (bs->AllocatePool(EFI_MEMORY_TYPE_LOADER_DATA, size,
                             (void **)&map) == EFI_SUCCESS &&
            bs->GetMemoryMap(&size, map, &key, &desc_size,
                             &desc_ver) == EFI_SUCCESS) {
            for (off = 0; off + sizeof(EFI_MEMORY_DESCRIPTOR) <= size;
                 off += desc_size) {
                const EFI_MEMORY_DESCRIPTOR *d =
                    (const EFI_MEMORY_DESCRIPTOR *)(map + off);
                if (is_ram_type(d->Type))
                    pages += d->NumberOfPages;
            }
        }
        if (map != 0)
            bs->FreePool(map);
    }
    if (pages == 0) {
        bb_sb_str(&v, "unknown");
        bb_info_add(BB_INFO_WARN, BB_INFO_LABEL_MEMORY, &v);
        return;
    }
    mib = (unsigned)(pages >> 8);           /* 4 KiB pages -> MiB */
    if (mib >= 1024u) {
        bb_sb_uint(&v, mib / 1024u);
        bb_sb_str(&v, ".");
        bb_sb_uint(&v, (mib % 1024u) * 10u / 1024u);
        bb_sb_str(&v, " GiB");
    } else {
        bb_sb_uint(&v, mib);
        bb_sb_str(&v, " MiB");
    }
    bb_info_add(BB_INFO_OK, BB_INFO_LABEL_MEMORY, &v);
}
#endif

#if BB_INFO_VIDEO
static const char *pci_vendor(unsigned id)
{
    switch (id) {
    case 0x8086: return "Intel";
    case 0x10DE: return "NVIDIA";
    case 0x1002: return "AMD/ATI";
    case 0x1022: return "AMD";
    case 0x1234: return "QEMU/Bochs";
    case 0x1AF4: return "Red Hat (virtio)";
    case 0x1B36: return "Red Hat (QEMU)";
    case 0x15AD: return "VMware";
    case 0x80EE: return "VirtualBox";
    case 0x1414: return "Microsoft";
    case 0x1A03: return "ASPEED";
    default:     return "PCI vendor";
    }
}

/* First display controller (PCI base class 3) on any PCI bus. */
static int find_video_adapter(EFI_SYSTEM_TABLE *st, unsigned *vendor,
                              unsigned *device)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_HANDLE *handles = 0;
    UINTN size = 0, i, count;
    int found = 0;

    if (bs->LocateHandle(EFI_LOCATE_BY_PROTOCOL, &guid_pci_io, 0, &size,
                         0) != EFI_BUFFER_TOO_SMALL)
        return 0;
    if (bs->AllocatePool(EFI_MEMORY_TYPE_LOADER_DATA, size,
                         (void **)&handles) != EFI_SUCCESS)
        return 0;
    if (bs->LocateHandle(EFI_LOCATE_BY_PROTOCOL, &guid_pci_io, 0, &size,
                         handles) == EFI_SUCCESS) {
        count = size / sizeof(EFI_HANDLE);
        for (i = 0; i < count && !found; i++) {
            EFI_PCI_IO_PROTOCOL *pci = 0;
            uint32_t id = 0, class_rev = 0;

            if (bs->HandleProtocol(handles[i], &guid_pci_io,
                                   (void **)&pci) != EFI_SUCCESS || pci == 0)
                continue;
            if (pci->Pci.Read(pci, EFI_PCI_IO_WIDTH_UINT32, 0x00, 1, &id) !=
                    EFI_SUCCESS ||
                pci->Pci.Read(pci, EFI_PCI_IO_WIDTH_UINT32, 0x08, 1,
                              &class_rev) != EFI_SUCCESS)
                continue;
            if ((class_rev >> 24) == 0x03u) {
                *vendor = id & 0xFFFFu;
                *device = id >> 16;
                found = 1;
            }
        }
    }
    bs->FreePool(handles);
    return found;
}

static void info_video(EFI_SYSTEM_TABLE *st)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    unsigned vendor, device;
    struct bb_sb v;

    bb_sb_init(&v);
    if (find_video_adapter(st, &vendor, &device)) {
        bb_sb_str(&v, pci_vendor(vendor));
        bb_sb_str(&v, " [");
        bb_sb_hex(&v, vendor, 4);
        bb_sb_str(&v, ":");
        bb_sb_hex(&v, device, 4);
        bb_sb_str(&v, "]");
        bb_info_add(BB_INFO_OK, BB_INFO_LABEL_ADAPTER, &v);
    } else {
        bb_sb_str(&v, "not found on PCI");
        bb_info_add(BB_INFO_WARN, BB_INFO_LABEL_ADAPTER, &v);
    }

    bb_sb_init(&v);
    if (st->BootServices->LocateProtocol(&guid_gop, 0, (void **)&gop) ==
            EFI_SUCCESS && gop != 0 && gop->Mode != 0 &&
        gop->Mode->Info != 0) {
        bb_sb_uint(&v, gop->Mode->Info->HorizontalResolution);
        bb_sb_str(&v, "x");
        bb_sb_uint(&v, gop->Mode->Info->VerticalResolution);
        bb_sb_str(&v, " (graphics output)");
        bb_info_add(BB_INFO_OK, BB_INFO_LABEL_DISPLAY, &v);
    } else {
        bb_sb_str(&v, "text console only");
        bb_info_add(BB_INFO_WARN, BB_INFO_LABEL_DISPLAY, &v);
    }
}
#endif

unsigned uefi_info_show(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                        const struct bb_console *con)
{
    (void)self;
#if BB_INFO_MEMORY
    info_memory(st);
#endif
    uefi_info_cpu();
#if BB_INFO_VIDEO
    info_video(st);
#else
    (void)st;
#endif
    if (BB_INFO_SECONDS == 0)
        return 0;               /* collected, but nothing to show */
    return bb_info_draw(con);
}
