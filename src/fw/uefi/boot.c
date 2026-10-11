/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "boot.h"
#include "console.h"
#include "error.h"
#include "log.h"

/* GUIDs are from the UEFI specification and the Linux EFI stub ABI. */
static const EFI_GUID guid_loaded_image = {
    0x5B1B31A1, 0x9562, 0x11d2,
    {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
static const EFI_GUID guid_simple_fs = {
    0x964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
static const EFI_GUID guid_file_info = {
    0x09576e92, 0x6d3f, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
static const EFI_GUID guid_device_path = {
    0x09576e91, 0x6d3f, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
static const EFI_GUID guid_load_file2 = {
    0x4006c0c1, 0xfcb3, 0x403e,
    {0x99, 0x6d, 0x4a, 0x6c, 0x87, 0x24, 0xe0, 0x6d}};

#define MAX_PATH_CHARS 255u

/* Records the failure for the error screen and in the debug log. */
static EFI_STATUS fail(const char *what, const char *detail, EFI_STATUS s)
{
    bb_error_set(what, detail, s);
    BB_LOG_STATUS(what, s);
    return s;
}

/* ------------------------------------------------------------------ */
/* Running an EFI image that is already in memory                      */
/* ------------------------------------------------------------------ */

/*
 * The firmware's own PE loader relocates the image from memory, so no
 * decompression or parsing is needed here. devpath (may be NULL) tells the
 * image where it "came from", which UKI stubs use to find their volume.
 * Under Secure Boot the image must be signed or LoadImage will refuse it.
 */
static EFI_STATUS run_image(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                            void *data, UINTN size, void *devpath,
                            const char *load_err, const char *ret_err,
                            const char *detail)
{
    EFI_HANDLE child = 0;
    UINTN exit_size = 0;
    CHAR16 *exit_data = 0;
    EFI_STATUS status;

    status = st->BootServices->LoadImage(0, self, devpath, data, size, &child);
    if (status != EFI_SUCCESS)
        return fail(load_err, detail, status);

    status = st->BootServices->StartImage(child, &exit_size, &exit_data);
    BB_LOG_STATUS("BinBoot: StartImage returned", status);
    if (status != EFI_SUCCESS)
        return fail(ret_err, detail, status);
    return EFI_SUCCESS;
}

EFI_STATUS uefi_boot_blob(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                          const struct bb_blob *blob)
{
    /* The firmware only reads the buffer, so dropping const is safe. */
    return run_image(self, st, (void *)blob->data, (UINTN)blob->size, 0,
                     "Firmware refused to load the embedded image",
                     "The embedded image returned an error", 0);
}

/* ------------------------------------------------------------------ */
/* File access on the boot volume                                      */
/* ------------------------------------------------------------------ */

struct file_buf {
    void *data;
    UINTN size;
};

/* Converts "/boot/vmlinuz" to the UEFI form "\boot\vmlinuz" in UCS-2. */
static EFI_STATUS to_uefi_path(const char *path, CHAR16 *out)
{
    UINTN n = 0;

    for (; *path != '\0'; path++) {
        if (n >= MAX_PATH_CHARS)
            return EFI_INVALID_PARAMETER;
        out[n++] = (*path == '/') ? (CHAR16)'\\' : (CHAR16)(unsigned char)*path;
    }
    out[n] = 0;
    return EFI_SUCCESS;
}

/* ---- choosing a volume by PARTUUID ---- */

struct volume_id {
    int kind;               /* 1 = MBR (signature + partition), 2 = GPT */
    uint8_t guid[16];       /* GPT: raw GUID bytes as stored on disk */
    uint32_t signature;     /* MBR: disk signature */
    uint32_t partition;     /* MBR: partition number */
};

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Parses `digits` hex digits into *out; returns 0 on bad input. */
static int parse_hex(const char *s, unsigned digits, uint32_t *out)
{
    uint32_t v = 0;
    unsigned i;

    for (i = 0; i < digits; i++) {
        int d = hex_digit(s[i]);
        if (d < 0)
            return 0;
        v = (v << 4) | (uint32_t)d;
    }
    *out = v;
    return 1;
}

static unsigned str_len(const char *s)
{
    unsigned n = 0;

    while (s[n] != '\0')
        n++;
    return n;
}

static int parse_partuuid(const char *s, struct volume_id *v)
{
    unsigned len = str_len(s), i;
    uint32_t d1, d2, d3, b;

    if (len == 36 && s[8] == '-' && s[13] == '-' && s[18] == '-' &&
        s[23] == '-') {
        if (!parse_hex(s, 8, &d1) || !parse_hex(s + 9, 4, &d2) ||
            !parse_hex(s + 14, 4, &d3))
            return 0;
        /* On disk the first three groups are little-endian. */
        v->guid[0] = (uint8_t)d1;
        v->guid[1] = (uint8_t)(d1 >> 8);
        v->guid[2] = (uint8_t)(d1 >> 16);
        v->guid[3] = (uint8_t)(d1 >> 24);
        v->guid[4] = (uint8_t)d2;
        v->guid[5] = (uint8_t)(d2 >> 8);
        v->guid[6] = (uint8_t)d3;
        v->guid[7] = (uint8_t)(d3 >> 8);
        for (i = 0; i < 8; i++) {
            /* groups 4 and 5 are plain byte sequences */
            const char *p = (i < 2) ? s + 19 + 2 * i : s + 24 + 2 * (i - 2);
            if (!parse_hex(p, 2, &b))
                return 0;
            v->guid[8 + i] = (uint8_t)b;
        }
        v->kind = 2;
        return 1;
    }
    if (len == 11 && s[8] == '-') {
        if (!parse_hex(s, 8, &v->signature) || !parse_hex(s + 9, 2, &b))
            return 0;
        v->partition = b;
        v->kind = 1;
        return 1;
    }
    return 0;
}

static int device_path_matches(const EFI_DEVICE_PATH_PROTOCOL *dp,
                               const struct volume_id *v)
{
    while (!(dp->Type == 0x7F && dp->SubType == 0xFF)) {
        if (dp->Length < sizeof(*dp))
            return 0;                       /* corrupt path, stop */
        if (dp->Type == 0x04 && dp->SubType == 0x01) {
            const EFI_HARDDRIVE_DEVICE_PATH *hd =
                (const EFI_HARDDRIVE_DEVICE_PATH *)dp;
            unsigned i;

            /* Debug aid: shows what the firmware reports for each volume. */
            BB_LOG_STATUS("BinBoot: volume sigtype", hd->SignatureType);
            BB_LOG_STATUS("BinBoot: volume partition", hd->PartitionNumber);
            BB_LOG_STATUS("BinBoot: volume sig[0..7]",
                          *(const uint64_t *)hd->Signature);

            if (v->kind == 2 && hd->SignatureType == 2) {
                for (i = 0; i < 16; i++)
                    if (hd->Signature[i] != v->guid[i])
                        return 0;
                return 1;
            }
            if (v->kind == 1 && hd->SignatureType == 1) {
                uint32_t sig = (uint32_t)hd->Signature[0] |
                               ((uint32_t)hd->Signature[1] << 8) |
                               ((uint32_t)hd->Signature[2] << 16) |
                               ((uint32_t)hd->Signature[3] << 24);
                return sig == v->signature &&
                       hd->PartitionNumber == v->partition;
            }
            return 0;
        }
        dp = (const EFI_DEVICE_PATH_PROTOCOL *)((const uint8_t *)dp + dp->Length);
    }
    return 0;
}

/* Finds the filesystem handle whose partition matches the PARTUUID. */
static EFI_STATUS find_volume(EFI_SYSTEM_TABLE *st, const char *partuuid,
                              EFI_HANDLE *out)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    struct volume_id id;
    EFI_HANDLE *handles = 0;
    UINTN size = 0, count, i;
    EFI_STATUS status;
    EFI_STATUS result = EFI_NOT_FOUND;

    if (!parse_partuuid(partuuid, &id))
        return EFI_INVALID_PARAMETER;

    status = bs->LocateHandle(EFI_LOCATE_BY_PROTOCOL, &guid_simple_fs, 0,
                              &size, 0);
    if (status != EFI_BUFFER_TOO_SMALL)
        return status;
    status = bs->AllocatePool(EFI_MEMORY_TYPE_LOADER_DATA, size,
                              (void **)&handles);
    if (status != EFI_SUCCESS)
        return status;
    status = bs->LocateHandle(EFI_LOCATE_BY_PROTOCOL, &guid_simple_fs, 0,
                              &size, handles);
    if (status != EFI_SUCCESS) {
        bs->FreePool(handles);
        return status;
    }

    count = size / sizeof(EFI_HANDLE);
    for (i = 0; i < count; i++) {
        EFI_DEVICE_PATH_PROTOCOL *dp = 0;

        if (bs->HandleProtocol(handles[i], &guid_device_path,
                               (void **)&dp) != EFI_SUCCESS || dp == 0)
            continue;
        if (device_path_matches(dp, &id)) {
            *out = handles[i];
            result = EFI_SUCCESS;
            break;
        }
    }
    bs->FreePool(handles);
    return result;
}

/* Opens the root of `volume` (PARTUUID), or of BinBoot's own volume if
 * volume is NULL. On success *vol_handle is the filesystem handle. */
static EFI_STATUS open_volume(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                              const char *volume, EFI_FILE_PROTOCOL **root,
                              EFI_HANDLE *vol_handle)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = 0;
    EFI_STATUS status;

    if (volume == NULL) {
        EFI_LOADED_IMAGE_PROTOCOL *li = 0;

        status = bs->HandleProtocol(self, &guid_loaded_image, (void **)&li);
        if (status != EFI_SUCCESS)
            return status;
        *vol_handle = li->DeviceHandle;
    } else {
        status = find_volume(st, volume, vol_handle);
        if (status != EFI_SUCCESS)
            return status;
    }
    status = bs->HandleProtocol(*vol_handle, &guid_simple_fs, (void **)&fs);
    if (status != EFI_SUCCESS)
        return status;
    return fs->OpenVolume(fs, root);
}

/* Reads a whole file into freshly allocated loader-data memory. */
static EFI_STATUS read_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                            const char *path, struct file_buf *out)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    CHAR16 path16[MAX_PATH_CHARS + 1];
    EFI_FILE_PROTOCOL *file = 0;
    EFI_FILE_INFO_HEADER *info = 0;
    UINTN info_size = 0, got, total = 0;
    uint8_t *buf = 0;
    EFI_STATUS status;

    status = to_uefi_path(path, path16);
    if (status != EFI_SUCCESS)
        return status;
    status = root->Open(root, &file, path16, EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS)
        return status;

    /* First call reports the needed size; second call fills it in. */
    status = file->GetInfo(file, &guid_file_info, &info_size, 0);
    if (status != EFI_BUFFER_TOO_SMALL)
        goto fail;
    status = bs->AllocatePool(EFI_MEMORY_TYPE_LOADER_DATA, info_size,
                              (void **)&info);
    if (status != EFI_SUCCESS)
        goto fail;
    status = file->GetInfo(file, &guid_file_info, &info_size, info);
    if (status != EFI_SUCCESS)
        goto fail;
    out->size = (UINTN)info->FileSize;
    bs->FreePool(info);
    info = 0;

    status = bs->AllocatePool(EFI_MEMORY_TYPE_LOADER_DATA, out->size,
                              (void **)&buf);
    if (status != EFI_SUCCESS)
        goto fail;

    /* Read in a loop: a single call may return fewer bytes than asked. */
    while (total < out->size) {
        got = out->size - total;
        status = file->Read(file, &got, buf + total);
        if (status != EFI_SUCCESS)
            goto fail;
        if (got == 0) {
            status = EFI_LOAD_ERROR;
            goto fail;
        }
        total += got;
    }
    file->Close(file);
    out->data = buf;
    return EFI_SUCCESS;

fail:
    if (info != 0)
        bs->FreePool(info);
    if (buf != 0)
        bs->FreePool(buf);
    file->Close(file);
    return status;
}

/* ------------------------------------------------------------------ */
/* Initrd handover through EFI_LOAD_FILE2_PROTOCOL                     */
/* ------------------------------------------------------------------ */

static struct file_buf g_initrd;

static EFI_STATUS EFIAPI initrd_load_file(EFI_LOAD_FILE2_PROTOCOL *This,
                                          void *FilePath, BOOLEAN BootPolicy,
                                          UINTN *BufferSize, void *Buffer)
{
    const uint8_t *src = (const uint8_t *)g_initrd.data;
    uint8_t *dst = (uint8_t *)Buffer;
    UINTN i;

    (void)This;
    (void)FilePath;
    if (BootPolicy)
        return EFI_UNSUPPORTED;
    if (Buffer == 0 || *BufferSize < g_initrd.size) {
        *BufferSize = g_initrd.size;
        return EFI_BUFFER_TOO_SMALL;
    }
    for (i = 0; i < g_initrd.size; i++)
        dst[i] = src[i];
    *BufferSize = g_initrd.size;
    return EFI_SUCCESS;
}

static EFI_LOAD_FILE2_PROTOCOL g_load_file2 = { initrd_load_file };

/* Linux finds the initrd through this vendor-media device path
 * (LINUX_EFI_INITRD_MEDIA_GUID), followed by the end node. */
static struct {
    EFI_DEVICE_PATH_PROTOCOL vendor;
    EFI_GUID guid;
    EFI_DEVICE_PATH_PROTOCOL end;
} __attribute__((packed)) g_initrd_path = {
    { 0x04, 0x03, 20 },             /* MEDIA_DEVICE_PATH, MEDIA_VENDOR */
    { 0x5568e427, 0x68fc, 0x4f3d,
      {0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68} },
    { 0x7F, 0xFF, 4 }               /* END_ENTIRE_DEVICE_PATH */
};

static EFI_STATUS install_initrd(EFI_BOOT_SERVICES *bs, EFI_HANDLE *handle)
{
    EFI_STATUS status;

    *handle = 0;
    status = bs->InstallProtocolInterface(handle, &guid_device_path,
                                          EFI_NATIVE_INTERFACE,
                                          &g_initrd_path);
    if (status != EFI_SUCCESS)
        return status;
    status = bs->InstallProtocolInterface(handle, &guid_load_file2,
                                          EFI_NATIVE_INTERFACE,
                                          &g_load_file2);
    if (status != EFI_SUCCESS)
        bs->UninstallProtocolInterface(*handle, &guid_device_path,
                                       &g_initrd_path);
    return status;
}

/* ------------------------------------------------------------------ */
/* Linux boot                                                          */
/* ------------------------------------------------------------------ */

/* Builds the UCS-2 load options string the Linux EFI stub reads. */
static EFI_STATUS set_load_options(EFI_SYSTEM_TABLE *st, EFI_HANDLE child,
                                   const char *cmdline)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_LOADED_IMAGE_PROTOCOL *li = 0;
    CHAR16 *opts = 0;
    UINTN len = 0, i;
    EFI_STATUS status;

    while (cmdline[len] != '\0')
        len++;
    status = bs->HandleProtocol(child, &guid_loaded_image, (void **)&li);
    if (status != EFI_SUCCESS)
        return status;
    status = bs->AllocatePool(EFI_MEMORY_TYPE_LOADER_DATA,
                              (len + 1) * sizeof(CHAR16), (void **)&opts);
    if (status != EFI_SUCCESS)
        return status;
    for (i = 0; i < len; i++)
        opts[i] = (CHAR16)(unsigned char)cmdline[i];
    opts[len] = 0;
    li->LoadOptions = opts;
    li->LoadOptionsSize = (uint32_t)((len + 1) * sizeof(CHAR16));
    return EFI_SUCCESS;
}

EFI_STATUS uefi_boot_linux(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                           const struct bb_entry *entry)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_FILE_PROTOCOL *root = 0;
    struct file_buf kernel = { 0, 0 };
    EFI_HANDLE child = 0, initrd_handle = 0, volume = 0;
    UINTN exit_size = 0;
    CHAR16 *exit_data = 0;
    int have_initrd = 0;
    EFI_STATUS status;

    status = open_volume(self, st, entry->volume, &root, &volume);
    if (status != EFI_SUCCESS)
        return fail("Cannot find or open the volume", entry->volume, status);

    status = read_file(st, root, entry->kernel, &kernel);
    if (status != EFI_SUCCESS) {
        root->Close(root);
        return fail("Cannot read the kernel file", entry->kernel, status);
    }

    if (entry->initrd != NULL) {
        g_initrd.data = 0;
        g_initrd.size = 0;
        status = read_file(st, root, entry->initrd, &g_initrd);
        if (status != EFI_SUCCESS) {
            root->Close(root);
            bs->FreePool(kernel.data);
            return fail("Cannot read the initrd file", entry->initrd, status);
        }
        root->Close(root);
        status = install_initrd(bs, &initrd_handle);
        if (status != EFI_SUCCESS) {
            bs->FreePool(kernel.data);
            bs->FreePool(g_initrd.data);
            return fail("Cannot register the initrd with the firmware", 0,
                        status);
        }
        have_initrd = 1;
    }

    if (entry->initrd == NULL)
        root->Close(root);
    status = bs->LoadImage(0, self, 0, kernel.data, kernel.size, &child);
    bs->FreePool(kernel.data);      /* The firmware made its own copy. */
    if (status != EFI_SUCCESS) {
        fail("Firmware refused to load the kernel", entry->kernel, status);
        goto out;
    }

    if (entry->cmdline != NULL) {
        status = set_load_options(st, child, entry->cmdline);
        if (status != EFI_SUCCESS) {
            fail("Cannot set the kernel command line", 0, status);
            goto out;
        }
    }

    status = bs->StartImage(child, &exit_size, &exit_data);
    BB_LOG_STATUS("BinBoot: kernel returned", status);
    if (status != EFI_SUCCESS)
        fail("The kernel returned an error", entry->kernel, status);

out:
    if (have_initrd) {
        bs->UninstallProtocolInterface(initrd_handle, &guid_load_file2,
                                       &g_load_file2);
        bs->UninstallProtocolInterface(initrd_handle, &guid_device_path,
                                       &g_initrd_path);
        bs->FreePool(g_initrd.data);
    }
    return status;
}

/* ------------------------------------------------------------------ */
/* EFI binary / UKI from a file                                        */
/* ------------------------------------------------------------------ */

/*
 * Builds "<volume device path> + File Path node + end" for LoadImage, so the
 * loaded image gets a correct DeviceHandle/FilePath (UKI stubs use these to
 * find the volume they live on). Memory comes from the pool; caller frees.
 */
static EFI_STATUS build_file_devpath(EFI_SYSTEM_TABLE *st, EFI_HANDLE volume,
                                     const CHAR16 *path16, void **out)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_DEVICE_PATH_PROTOCOL *vdp = 0, *node;
    uint8_t *buf = 0;
    UINTN vlen = 0, plen = 0, i, total;
    EFI_STATUS status;

    status = bs->HandleProtocol(volume, &guid_device_path, (void **)&vdp);
    if (status != EFI_SUCCESS || vdp == 0)
        return status != EFI_SUCCESS ? status : EFI_NOT_FOUND;

    for (node = vdp; !(node->Type == 0x7F && node->SubType == 0xFF);
         node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)node + node->Length)) {
        if (node->Length < sizeof(*node))
            return EFI_INVALID_PARAMETER;
        vlen += node->Length;
    }
    while (path16[plen] != 0)
        plen++;
    plen = (plen + 1) * sizeof(CHAR16);         /* include the terminator */

    total = vlen + sizeof(EFI_DEVICE_PATH_PROTOCOL) + plen +
            sizeof(EFI_DEVICE_PATH_PROTOCOL);
    status = bs->AllocatePool(EFI_MEMORY_TYPE_LOADER_DATA, total,
                              (void **)&buf);
    if (status != EFI_SUCCESS)
        return status;

    for (i = 0; i < vlen; i++)
        buf[i] = ((uint8_t *)vdp)[i];
    node = (EFI_DEVICE_PATH_PROTOCOL *)(buf + vlen);
    node->Type = 0x04;                          /* MEDIA_DEVICE_PATH */
    node->SubType = 0x04;                       /* MEDIA_FILEPATH_DP */
    node->Length = (uint16_t)(sizeof(*node) + plen);
    for (i = 0; i < plen; i++)
        buf[vlen + sizeof(*node) + i] = ((const uint8_t *)path16)[i];
    node = (EFI_DEVICE_PATH_PROTOCOL *)(buf + vlen + sizeof(*node) + plen);
    node->Type = 0x7F;                          /* END_DEVICE_PATH */
    node->SubType = 0xFF;                       /* END_ENTIRE */
    node->Length = sizeof(*node);
    *out = buf;
    return EFI_SUCCESS;
}

EFI_STATUS uefi_boot_uki(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                         const struct bb_entry *entry)
{
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_FILE_PROTOCOL *root = 0;
    EFI_HANDLE volume = 0;
    struct file_buf file = { 0, 0 };
    CHAR16 path16[MAX_PATH_CHARS + 1];
    void *devpath = 0;
    EFI_STATUS status;

    status = open_volume(self, st, entry->volume, &root, &volume);
    if (status != EFI_SUCCESS)
        return fail("Cannot find or open the volume", entry->volume, status);

    status = read_file(st, root, entry->uki_path, &file);
    root->Close(root);
    if (status != EFI_SUCCESS)
        return fail("Cannot read the EFI file", entry->uki_path, status);

    status = to_uefi_path(entry->uki_path, path16);
    if (status == EFI_SUCCESS)
        status = build_file_devpath(st, volume, path16, &devpath);
    if (status != EFI_SUCCESS) {
        bs->FreePool(file.data);
        return fail("Cannot build the device path of the EFI file",
                    entry->uki_path, status);
    }

    status = run_image(self, st, file.data, file.size, devpath,
                       "Firmware refused to load the EFI file",
                       "The EFI file returned an error", entry->uki_path);
    bs->FreePool(devpath);
    bs->FreePool(file.data);
    return status;
}
