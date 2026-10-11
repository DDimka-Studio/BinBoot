/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Legacy BIOS firmware layer (real mode, 16-bit code, flat segment 0). */
#ifndef BB_BIOS_H
#define BB_BIOS_H

#include "menu.h"

/* BIOS drive number we were booted from (set by start.S). */
extern unsigned char bb_boot_drive;

const struct bb_console *bios_console_init(void);
/* Restores a standard text mode and cursor before handing over. */
void bios_video_restore(void);

#ifdef BB_LOG_ENABLED
void bios_serial_init(void);
void bios_serial_write(const char *s);
#endif

#endif /* BB_BIOS_H */
