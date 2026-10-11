/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#ifndef BB_BIOS_DISK_H
#define BB_BIOS_DISK_H

/*
 * Reads `count` sectors starting at `lba` with INT 13h extensions to
 * seg:off (the buffer must not cross a 64 KiB boundary).
 * Returns 0 on success, otherwise the BIOS status code (never 0).
 */
unsigned bb_disk_read(unsigned char drive, unsigned lba, unsigned count,
                      unsigned seg, unsigned off);

#endif /* BB_BIOS_DISK_H */
