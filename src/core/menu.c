/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "menu.h"
#include "binboot.h"
#include "config.h"

/* Layout: marker in column 0, entry name from column 2, entries from row 0. */
#define MARKER_COL 0
#define NAME_COL   2
#define TICK_MS    50u

static void draw_entry(const struct bb_console *con, unsigned top,
                       size_t i, int selected)
{
    con->put(MARKER_COL, top + (unsigned)i, selected ? ">" : " ");
    con->put(NAME_COL, top + (unsigned)i, bb_entries[i].name);
}

/* Writes the decimal form of v into buf; returns the new write position. */
static size_t put_uint(char *buf, size_t pos, unsigned v)
{
    char tmp[10];
    size_t n = 0;

    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    while (n > 0)
        buf[pos++] = tmp[--n];
    return pos;
}

static void draw_countdown(const struct bb_console *con, unsigned row,
                           unsigned seconds)
{
    char line[64];
    size_t pos = 0;
    const char *p;

    for (p = BB_TIMEOUT_PREFIX; *p != '\0' && pos < 24; p++)
        line[pos++] = *p;
    pos = put_uint(line, pos, seconds);
    for (p = BB_TIMEOUT_SUFFIX; *p != '\0' && pos < sizeof(line) - 1; p++)
        line[pos++] = *p;
    line[pos] = '\0';

    /* Clear first: going from "10" to "9" would leave a stale digit. */
    con->clear_line(row);
    con->put(0, row, line);
}

size_t bb_menu_run(const struct bb_console *con, int autoboot,
                   unsigned first_row)
{
    size_t sel, i, visible;
    unsigned rows, remaining, elapsed_ms = 0;
    int counting;

    if (bb_entry_count == 0)
        return BB_MENU_NONE;

    sel = (BB_DEFAULT_ENTRY < bb_entry_count) ? BB_DEFAULT_ENTRY : 0;
    if (autoboot && BB_TIMEOUT_SECONDS == 0)
        return sel;                       /* Boot at once, no menu */

    rows = con->rows();
    if (first_row + 2 > rows)
        first_row = 0;                    /* no room below the text above */
    /* Keep the bottom row free for the countdown; clip what does not fit. */
    visible = bb_entry_count;
    if (rows >= 2 && visible > rows - 1 - first_row)
        visible = rows - 1 - first_row;

    BB_SET_COLOR(con, BB_COLOR_TEXT_FG, BB_COLOR_TEXT_BG);
    if (first_row == 0)
        con->clear();
    for (i = 0; i < visible; i++)
        draw_entry(con, first_row, i, i == sel);

    counting = autoboot;
    remaining = BB_TIMEOUT_SECONDS;
    if (counting)
        draw_countdown(con, rows - 1, remaining);

    for (;;) {
        enum bb_key key = con->poll_key();

        if (key == BB_KEY_NONE) {
            con->sleep_ms(TICK_MS);
            if (!counting)
                continue;
            /*
             * Time is counted in nominal sleep ticks instead of reading a
             * clock: it needs no extra firmware service and drifts by only
             * a few percent, which is irrelevant for a boot timeout.
             */
            elapsed_ms += TICK_MS;
            if (elapsed_ms >= 1000u) {
                elapsed_ms -= 1000u;
                if (--remaining == 0u)
                    return sel;
                draw_countdown(con, rows - 1, remaining);
            }
            continue;
        }

        if (counting) {                   /* Any key cancels autoboot */
            counting = 0;
            con->clear_line(rows - 1);
        }

        if (key == BB_KEY_ENTER)
            return sel;
        if (key == BB_KEY_UP && sel > 0) {
            draw_entry(con, first_row, sel, 0);
            draw_entry(con, first_row, --sel, 1);
        } else if (key == BB_KEY_DOWN && sel + 1 < visible) {
            draw_entry(con, first_row, sel, 0);
            draw_entry(con, first_row, ++sel, 1);
        }
    }
}
