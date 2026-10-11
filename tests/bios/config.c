/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* BIOS test configuration: two good entries and one empty partition. */
#include "binboot.h"

const struct bb_entry bb_entries[] = {
    { .name = "Partition 1",
      .chain = { .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE, .partition = 1 } },
    { .name = "Partition 2",
      .chain = { .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE, .partition = 2 } },
    { .name = "Empty partition 4",
      .chain = { .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE, .partition = 4 } },
};

const size_t bb_entry_count = sizeof(bb_entries) / sizeof(bb_entries[0]);
