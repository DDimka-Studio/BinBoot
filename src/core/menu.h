/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Firmware-independent menu. The firmware layer supplies a console. */
#ifndef BB_MENU_H
#define BB_MENU_H

#include <stddef.h>

#include "config.h"

enum bb_key { BB_KEY_NONE, BB_KEY_UP, BB_KEY_DOWN, BB_KEY_ENTER, BB_KEY_PAUSE,
              BB_KEY_OTHER };

struct bb_console {
    void         (*clear)(void);
    /* Writes an ASCII string at (col, row); both are zero-based. */
    void         (*put)(unsigned col, unsigned row, const char *s);
    void         (*clear_line)(unsigned row);
    unsigned     (*rows)(void);
    enum bb_key  (*poll_key)(void);   /* Non-blocking */
    void         (*sleep_ms)(unsigned ms);
    /* Colors for everything written afterwards (0..15 / 0..7). */
    void         (*set_attr)(unsigned fg, unsigned bg);
};

/* Color helper: compiles to nothing when BB_COLORS is 0. */
#if BB_COLORS
#define BB_SET_COLOR(con, fg, bg) ((con)->set_attr((fg), (bg)))
#else
#define BB_SET_COLOR(con, fg, bg) ((void)(con))
#endif

#define BB_MENU_NONE ((size_t)-1)

/*
 * Runs the menu and returns the chosen entry index, or BB_MENU_NONE.
 * autoboot != 0: honour BB_TIMEOUT_SECONDS. autoboot == 0: no countdown,
 * wait for the user (used after a failed boot attempt).
 * first_row > 0: the menu starts on that row and the screen is NOT cleared,
 * so text already drawn above it (the system information) stays visible.
 */
size_t bb_menu_run(const struct bb_console *con, int autoboot,
                   unsigned first_row);

#endif /* BB_MENU_H */
