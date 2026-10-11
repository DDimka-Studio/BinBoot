/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#ifndef BB_BIOS_LINUX_H
#define BB_BIOS_LINUX_H

#include "binboot.h"

/* Boots a Linux bzImage from raw sectors. Returns only on failure, with
 * the error recorded for the error screen. */
void bb_bios_linux_boot(const struct bb_bios_linux *cfg, const char *cmdline);

#endif /* BB_BIOS_LINUX_H */
