/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "boot.h"
#include "console.h"
#include "efi.h"
#include "binboot.h"
#include "error.h"
#include "log.h"
#include "menu.h"
#include "uefi_info.h"
#include "config.h"
#include "info.h"            /* src/core/info.h */

#ifdef BB_LOG_ENABLED
static void uefi_log_sink(const char *msg)
{
    uefi_out_ascii(msg);
    /* UEFI consoles need CR+LF; a bare LF only moves the cursor down. */
    uefi_out_ascii("\r\n");
}
#endif

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st)
{
    const struct bb_console *con;
    int autoboot = 1;

    con = uefi_console_init(st);
    BB_LOG_SET_SINK(uefi_log_sink);
    BB_LOG("BinBoot: started");

#if BB_BOOT_INFO
    /* The information gets its own screen; the menu replaces it. A key
     * pressed meanwhile skips the wait and cancels the automatic boot. */
    (void)uefi_info_show(image, st, con);
    if (BB_INFO_SECONDS > 0 && bb_info_wait(con, BB_INFO_SECONDS))
        autoboot = 0;
#endif

    for (;;) {
        const struct bb_entry *entry;
        EFI_STATUS status;
        size_t chosen = bb_menu_run(con, autoboot, 0);

        if (chosen == BB_MENU_NONE) {
            bb_error_set("No boot entries are configured", 0, BB_ERR_NOCODE);
            bb_error_screen(con, 0);
            continue;
        }

        entry = &bb_entries[chosen];
        if (entry->image != NULL) {
            BB_LOG("BinBoot: booting embedded image");
            status = uefi_boot_blob(image, st, entry->image);
        } else if (entry->uki_path != NULL) {
            BB_LOG("BinBoot: booting EFI binary from a file");
            status = uefi_boot_uki(image, st, entry);
        } else if (entry->kernel != NULL) {
            BB_LOG("BinBoot: booting Linux kernel");
            status = uefi_boot_linux(image, st, entry);
        } else {
            bb_error_set("This entry has nothing to boot", 0, BB_ERR_NOCODE);
            status = EFI_UNSUPPORTED;
        }

        if (status == EFI_SUCCESS)
            break;      /* An EFI application ran and exited normally. */

        /* Show the error until Enter; then stay in the menu, no timeout. */
        bb_error_screen(con, entry->name);
        autoboot = 0;
    }

    /* Keep output visible for a moment before returning to the firmware. */
    st->BootServices->Stall(3000000);
    return EFI_SUCCESS;
}
