/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#ifndef BB_UEFI_CONSOLE_H
#define BB_UEFI_CONSOLE_H

#include "efi.h"
#include "menu.h"

/* Binds the console to the firmware tables and returns its operations. */
const struct bb_console *uefi_console_init(EFI_SYSTEM_TABLE *st);

/* Prints an ASCII string at the current cursor position. */
void uefi_out_ascii(const char *s);

#ifdef BB_LOG_ENABLED
/* Logs "<what> 0x<status>" so firmware error codes are visible in debug. */
void uefi_log_status(const char *what, EFI_STATUS status);
#define BB_LOG_STATUS(what, status) uefi_log_status((what), (status))
#else
#define BB_LOG_STATUS(what, status) ((void)0)
#endif

#endif /* BB_UEFI_CONSOLE_H */
