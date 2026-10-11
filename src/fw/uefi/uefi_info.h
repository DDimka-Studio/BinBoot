/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#ifndef BB_UEFI_UEFI_INFO_H
#define BB_UEFI_UEFI_INFO_H

#include "efi.h"
#include "menu.h"

/* Collects and draws the system information; returns the rows used. */
unsigned uefi_info_show(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                        const struct bb_console *con);

/* Architecture-specific lines (x86: CPUID/MSR, ARM: system registers). */
void uefi_info_cpu(void);

#endif /* BB_UEFI_UEFI_INFO_H */
