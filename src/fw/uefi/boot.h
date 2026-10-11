/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#ifndef BB_UEFI_BOOT_H
#define BB_UEFI_BOOT_H

#include "binboot.h"
#include "efi.h"

/* Boots an EFI image held in memory (embedded UKI). */
EFI_STATUS uefi_boot_blob(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                          const struct bb_blob *blob);

/*
 * Boots an EFI binary (e.g. a UKI) from entry->uki_path on the volume
 * entry->volume (NULL = the volume BinBoot was loaded from).
 */
EFI_STATUS uefi_boot_uki(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                         const struct bb_entry *entry);

/*
 * Boots a Linux kernel with an EFI stub from the volume BinBoot was
 * loaded from: reads kernel (and initrd) through Simple File System,
 * passes cmdline as load options and the initrd via LoadFile2.
 * Returns only on failure.
 */
EFI_STATUS uefi_boot_linux(EFI_HANDLE self, EFI_SYSTEM_TABLE *st,
                           const struct bb_entry *entry);

#endif /* BB_UEFI_BOOT_H */
