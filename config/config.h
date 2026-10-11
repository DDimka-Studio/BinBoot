/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * BinBoot settings.
 *
 * This file holds the SETTINGS (timeouts, texts, colors, switches).
 * What the menu actually boots (the ENTRIES) is in config.c.
 *
 * Every option is a #define. Edit the value, save, rebuild:
 *     make build_uefi arch=native        or        make build_bios
 * Nothing is read at boot time: the values are compiled into the one
 * BinBoot file. See docs/configuration.md for a full description.
 *
 * Contents
 *   1. Menu and automatic boot
 *   2. System information screen   (UEFI only)
 *   3. Colors
 *   4. Boot error screen
 */
#ifndef BB_CONFIG_H
#define BB_CONFIG_H

/* ======================================================================
 * 1. Menu and automatic boot
 * ====================================================================== */

/* Which entry is selected at start: its position in config.c, counting
 * from 0 (the first entry is 0). */
#define BB_DEFAULT_ENTRY        0

/* Seconds until the selected entry boots by itself. Any key cancels the
 * countdown. 0 = boot the default entry at once, without showing a menu. */
#define BB_TIMEOUT_SECONDS      5

/* Legacy BIOS: the first menu entry is generated from `part=N` of the build
 * command (see config.c) and is named PREFIX + N. */
#define BB_PART_ENTRY_PREFIX    "Partition "

/* The countdown line on the bottom row reads: PREFIX + seconds + SUFFIX. */
#define BB_TIMEOUT_PREFIX       "Booting in "
#define BB_TIMEOUT_SUFFIX       " s"

/* ======================================================================
 * 2. System information screen (UEFI only; not in the Legacy BIOS build)
 *
 * A screen of its own shown before the menu:
 *     [  OK  ] Memory: 15.7 GiB
 *     [  OK  ] CPU: ...
 * ====================================================================== */

/* 1 = show it, 0 = never (everything below is then ignored). */
#define BB_BOOT_INFO            1

/* How many seconds it stays up before the menu replaces it. 0 = not shown.
 * Any key skips it, and also cancels the menu's automatic boot. */
#define BB_INFO_SECONDS         2

/* Pause/Break freezes the screen until the next key. Not every firmware
 * reports Pause, so one ordinary key can do the same: a character such as
 * 'p', or 0 for none. */
#define BB_INFO_HOLD_CHAR       0

/* Which lines to show (1 = yes, 0 = no). */
#define BB_INFO_MEMORY          1
#define BB_INFO_CPU             1   /* processor name */
#define BB_INFO_FEATURES        1   /* instruction set extensions */
#define BB_INFO_VIRT            1   /* VT-x / AMD-V state, or ARM EL level */
#define BB_INFO_VIDEO           1   /* video adapter and display mode */

/* Text of the status tags. Keep them the same width to keep rows aligned. */
#define BB_INFO_TAG_OK          "[  OK  ]"
#define BB_INFO_TAG_WARN        "[ WARN ]"
#define BB_INFO_TAG_FAIL        "[ FAIL ]"

/* Text in front of each value. */
#define BB_INFO_LABEL_MEMORY    "Memory: "
#define BB_INFO_LABEL_CPU       "CPU: "
#define BB_INFO_LABEL_FEATURES  "Features: "
#define BB_INFO_LABEL_VIRT      "Virtualization: "
#define BB_INFO_LABEL_ADAPTER   "Video adapter: "
#define BB_INFO_LABEL_DISPLAY   "Display: "

/* ======================================================================
 * 3. Colors (UEFI and Legacy BIOS text mode)
 *
 *   0 black      4 red        8 dark gray     12 light red
 *   1 blue       5 magenta    9 light blue    13 light magenta
 *   2 green      6 brown     10 light green   14 yellow
 *   3 cyan       7 light gray 11 light cyan    15 white
 *
 * Foreground colors use 0..15, background colors 0..7.
 * ====================================================================== */

/* 1 = use the colors below, 0 = leave the firmware's default colors. */
#define BB_COLORS               1

#define BB_COLOR_TEXT_FG        7   /* normal text */
#define BB_COLOR_TEXT_BG        0
#define BB_COLOR_INFO_OK        10  /* the [  OK  ] tag */
#define BB_COLOR_INFO_WARN      14  /* the [ WARN ] tag */
#define BB_COLOR_INFO_FAIL      12  /* the [ FAIL ] tag */
#define BB_COLOR_ERROR_TITLE    12  /* first line of the error screen */

/* ======================================================================
 * 4. Boot error screen
 *
 * When a boot attempt fails, a screen names the entry, what went wrong,
 * the file (if any) and the error code, and stays until Enter is pressed.
 * Then the menu returns, without an automatic-boot countdown.
 * ====================================================================== */

/* 1 = show it, 0 = skip it and go straight back to the menu. */
#define BB_ERROR_SCREEN         1

#define BB_ERROR_TITLE          "Boot failed"
#define BB_ERROR_CODE_LABEL     "Error code: "
#define BB_ERROR_PROMPT         "Press Enter to continue"

#endif /* BB_CONFIG_H */
