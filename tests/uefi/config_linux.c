/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Test configuration: native Linux boot from the ESP. */
#include "binboot.h"

const struct bb_entry bb_entries[] = {
    {
        .name    = "Linux (ESP)",
        .kernel  = "/EFI/linux/vmlinuz",
        .initrd  = "/EFI/linux/initrd.img",
        .cmdline = "console=ttyS0 rdinit=/init binboot=test",
    },
};

const size_t bb_entry_count = sizeof(bb_entries) / sizeof(bb_entries[0]);
