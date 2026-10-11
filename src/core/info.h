/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * System information lines shown above the menu ("[  OK  ] Memory: ...").
 * Firmware code collects lines with bb_info_add(); bb_info_draw() prints
 * them. Patches can add their own lines the same way before the draw.
 */
#ifndef BB_INFO_H
#define BB_INFO_H

#include "menu.h"

enum bb_info_status { BB_INFO_OK, BB_INFO_WARN, BB_INFO_FAIL };

#define BB_INFO_WIDTH 79u

/* A small string builder with a fixed buffer; overlong text is cut. */
struct bb_sb {
    char buf[BB_INFO_WIDTH + 1];
    unsigned len;
};

void bb_sb_init(struct bb_sb *sb);
void bb_sb_str(struct bb_sb *sb, const char *s);
void bb_sb_uint(struct bb_sb *sb, unsigned v);
void bb_sb_hex(struct bb_sb *sb, unsigned v, unsigned digits);

/* Adds "<status tag> <label><value>". */
void bb_info_add(enum bb_info_status status, const char *label,
                 const struct bb_sb *value);

/* Clears the screen, draws all lines and returns the number of rows used. */
unsigned bb_info_draw(const struct bb_console *con);

/*
 * Keeps the screen up for `seconds`. Returns 1 if the user pressed a key
 * (the caller should then not auto-boot). Pause/Break freezes the screen
 * until another key is pressed.
 */
int bb_info_wait(const struct bb_console *con, unsigned seconds);

#endif /* BB_INFO_H */
