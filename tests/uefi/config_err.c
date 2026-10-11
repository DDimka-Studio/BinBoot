/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Test configuration: an entry whose kernel does not exist. */
#include "binboot.h"

const struct bb_entry bb_entries[] = {
    { .name = "Missing kernel", .kernel = "/EFI/nothing/vmlinuz" },
};

const size_t bb_entry_count = sizeof(bb_entries) / sizeof(bb_entries[0]);
