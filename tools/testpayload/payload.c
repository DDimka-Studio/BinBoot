/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Minimal EFI app used to test embedding and file boot: prints a line,
 * reports whether it received a DeviceHandle, then returns. */
#include "efi.h"

static void say(EFI_SYSTEM_TABLE *st, const char *s)
{
    CHAR16 buf[96];
    unsigned n = 0;

    while (*s != '\0' && n < 90)
        buf[n++] = (CHAR16)(unsigned char)*s++;
    buf[n++] = 0x0D;
    buf[n++] = 0x0A;
    buf[n] = 0;
    st->ConOut->OutputString(st->ConOut, buf);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st)
{
    static const EFI_GUID guid_li = {
        0x5B1B31A1, 0x9562, 0x11d2,
        {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
    EFI_LOADED_IMAGE_PROTOCOL *li = 0;

    say(st, "Hello from embedded image");
    if (st->BootServices->HandleProtocol(image, &guid_li, (void **)&li) ==
        EFI_SUCCESS && li != 0)
        say(st, li->DeviceHandle != 0 ? "DEVHANDLE=1" : "DEVHANDLE=0");
    st->BootServices->Stall(2000000);
    return EFI_SUCCESS;
}
