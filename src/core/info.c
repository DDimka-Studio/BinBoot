/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "info.h"
#include "config.h"

#define MAX_LINES 12u

static struct bb_sb g_lines[MAX_LINES];
static enum bb_info_status g_status[MAX_LINES];
static unsigned g_count;

void bb_sb_init(struct bb_sb *sb)
{
    sb->len = 0;
    sb->buf[0] = '\0';
}

void bb_sb_str(struct bb_sb *sb, const char *s)
{
    while (s != 0 && *s != '\0' && sb->len < BB_INFO_WIDTH)
        sb->buf[sb->len++] = *s++;
    sb->buf[sb->len] = '\0';
}

void bb_sb_uint(struct bb_sb *sb, unsigned v)
{
    char tmp[10];
    unsigned n = 0;

    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    while (n > 0 && sb->len < BB_INFO_WIDTH)
        sb->buf[sb->len++] = tmp[--n];
    sb->buf[sb->len] = '\0';
}

void bb_sb_hex(struct bb_sb *sb, unsigned v, unsigned digits)
{
    static const char hex[] = "0123456789abcdef";

    while (digits > 0 && sb->len < BB_INFO_WIDTH) {
        sb->buf[sb->len++] = hex[(v >> (4u * --digits)) & 0xFu];
    }
    sb->buf[sb->len] = '\0';
}

void bb_info_add(enum bb_info_status status, const char *label,
                 const struct bb_sb *value)
{
    struct bb_sb *line;

    if (g_count >= MAX_LINES)
        return;
    g_status[g_count] = status;
    line = &g_lines[g_count++];
    bb_sb_init(line);               /* the status tag is drawn separately */
    bb_sb_str(line, label);
    bb_sb_str(line, value->buf);
}

static const char *tag_text(enum bb_info_status s)
{
    return s == BB_INFO_OK ? BB_INFO_TAG_OK
           : s == BB_INFO_WARN ? BB_INFO_TAG_WARN : BB_INFO_TAG_FAIL;
}

#if BB_COLORS
static unsigned tag_color(enum bb_info_status s)
{
    return s == BB_INFO_OK ? BB_COLOR_INFO_OK
           : s == BB_INFO_WARN ? BB_COLOR_INFO_WARN : BB_COLOR_INFO_FAIL;
}
#endif

unsigned bb_info_draw(const struct bb_console *con)
{
    unsigned i, rows = con->rows();

    BB_SET_COLOR(con, BB_COLOR_TEXT_FG, BB_COLOR_TEXT_BG);
    con->clear();
    for (i = 0; i < g_count && i + 2 < rows; i++) {
        const char *tag = tag_text(g_status[i]);
        unsigned tag_len = 0;

        while (tag[tag_len] != '\0')
            tag_len++;
#if BB_COLORS
        con->set_attr(tag_color(g_status[i]), BB_COLOR_TEXT_BG);
#endif
        con->put(0, i, tag);
        BB_SET_COLOR(con, BB_COLOR_TEXT_FG, BB_COLOR_TEXT_BG);
        con->put(tag_len + 1, i, g_lines[i].buf);
    }
    return i;
}

int bb_info_wait(const struct bb_console *con, unsigned seconds)
{
    unsigned ticks = seconds * 20u;         /* 50 ms steps */
    unsigned t;

    for (t = 0; t < ticks; t++) {
        enum bb_key key = con->poll_key();

        if (key == BB_KEY_NONE) {
            con->sleep_ms(50);
            continue;
        }
        if (key == BB_KEY_PAUSE) {
            /* Freeze: wait for the next key of any kind. */
            while (con->poll_key() == BB_KEY_NONE)
                con->sleep_ms(50);
        }
        return 1;
    }
    return 0;
}
