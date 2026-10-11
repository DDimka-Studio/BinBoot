/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "error.h"
#include "config.h"

static const char *g_what;
static const char *g_detail;
static unsigned long long g_code = BB_ERR_NOCODE;

void bb_error_set(const char *what, const char *detail,
                  unsigned long long code)
{
    g_what = what;
    g_detail = detail;
    g_code = code;
}

#if BB_ERROR_SCREEN
#define LINE_MAX_CHARS 79u

/* Appends s to line, never writing past LINE_MAX_CHARS. */
static void append(char *line, size_t *pos, const char *s)
{
    while (s != 0 && *s != '\0' && *pos < LINE_MAX_CHARS)
        line[(*pos)++] = *s++;
}

/* Appends "0x" and the value in hex without leading zeros. */
static void append_hex(char *line, size_t *pos, unsigned long long v)
{
    static const char digits[] = "0123456789ABCDEF";
    unsigned hi = (unsigned)(v >> 32), lo = (unsigned)v;
    int shift, started = 0;

    append(line, pos, "0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        unsigned d = (hi >> shift) & 0xFu;
        if (d != 0 || started) {
            if (*pos < LINE_MAX_CHARS)
                line[(*pos)++] = digits[d];
            started = 1;
        }
    }
    for (shift = 28; shift >= 0; shift -= 4) {
        unsigned d = (lo >> shift) & 0xFu;
        if (d != 0 || started || shift == 0) {
            if (*pos < LINE_MAX_CHARS)
                line[(*pos)++] = digits[d];
            started = 1;
        }
    }
}

static void put_line(const struct bb_console *con, unsigned row,
                     char *line, size_t len)
{
    line[len] = '\0';
    con->put(0, row, line);
}

#endif /* BB_ERROR_SCREEN: helpers above are only used by the screen */

void bb_error_screen(const struct bb_console *con, const char *entry_name)
{
#if BB_ERROR_SCREEN
    char line[LINE_MAX_CHARS + 1];
    size_t pos;
    unsigned row = 0;

    BB_SET_COLOR(con, BB_COLOR_TEXT_FG, BB_COLOR_TEXT_BG);
    con->clear();

    BB_SET_COLOR(con, BB_COLOR_ERROR_TITLE, BB_COLOR_TEXT_BG);
    pos = 0;
    append(line, &pos, BB_ERROR_TITLE);
    if (entry_name != 0) {
        append(line, &pos, ": ");
        append(line, &pos, entry_name);
    }
    put_line(con, row++, line, pos);
    BB_SET_COLOR(con, BB_COLOR_TEXT_FG, BB_COLOR_TEXT_BG);

    pos = 0;
    append(line, &pos, g_what != 0 ? g_what : "Unknown error");
    put_line(con, row++, line, pos);

    if (g_detail != 0) {
        pos = 0;
        append(line, &pos, g_detail);
        put_line(con, row++, line, pos);
    }
    if (g_code != BB_ERR_NOCODE) {
        pos = 0;
        append(line, &pos, BB_ERROR_CODE_LABEL);
        append_hex(line, &pos, g_code);
        put_line(con, row++, line, pos);
    }
    row++;                                  /* blank line */
    pos = 0;
    append(line, &pos, BB_ERROR_PROMPT);
    put_line(con, row, line, pos);

    /* Drop keys typed earlier (e.g. a repeated Enter) so the message
     * cannot vanish before it was seen. */
    while (con->poll_key() != BB_KEY_NONE)
        ;
    for (;;) {
        if (con->poll_key() == BB_KEY_ENTER)
            break;
        con->sleep_ms(50);
    }
#else
    (void)con;
    (void)entry_name;
#endif

    g_what = 0;
    g_detail = 0;
    g_code = BB_ERR_NOCODE;
}
