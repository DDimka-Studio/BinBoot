/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "console.h"
#include "config.h"
#include "log.h"

#define CHUNK 64u

static EFI_SYSTEM_TABLE *g_st;
static UINTN g_cols = 80, g_rows = 25;

void uefi_out_ascii(const char *s)
{
    CHAR16 buf[CHUNK + 1];

    /* The firmware wants UCS-2; the text is ASCII, so widening is enough.
     * Long strings are sent in chunks to keep the stack buffer small. */
    while (*s != '\0') {
        UINTN n = 0;

        while (*s != '\0' && n < CHUNK)
            buf[n++] = (CHAR16)(unsigned char)*s++;
        buf[n] = 0;
        g_st->ConOut->OutputString(g_st->ConOut, buf);
    }
}

static void c_clear(void)
{
    g_st->ConOut->ClearScreen(g_st->ConOut);
}

static void c_put(unsigned col, unsigned row, const char *s)
{
    g_st->ConOut->SetCursorPosition(g_st->ConOut, col, row);
    uefi_out_ascii(s);
}

static void c_clear_line(unsigned row)
{
    /* Spaces are written one cell short of the width: filling the very
     * last cell of the last row can scroll the whole screen. */
    char spaces[CHUNK + 1];
    UINTN left = g_cols - 1, i;

    for (i = 0; i < CHUNK; i++)
        spaces[i] = ' ';
    spaces[CHUNK] = '\0';

    g_st->ConOut->SetCursorPosition(g_st->ConOut, 0, row);
    while (left > 0) {
        if (left >= CHUNK) {
            uefi_out_ascii(spaces);
            left -= CHUNK;
        } else {
            spaces[left] = '\0';
            uefi_out_ascii(spaces);
            left = 0;
        }
    }
}

static unsigned c_rows(void)
{
    return (unsigned)g_rows;
}

static enum bb_key c_poll_key(void)
{
    EFI_INPUT_KEY key;

    /* NOT_READY simply means no key is waiting, which is the normal case. */
    if (g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) != EFI_SUCCESS)
        return BB_KEY_NONE;
    if (key.ScanCode == EFI_SCAN_UP)
        return BB_KEY_UP;
    if (key.ScanCode == EFI_SCAN_DOWN)
        return BB_KEY_DOWN;
    if (key.ScanCode == EFI_SCAN_PAUSE)
        return BB_KEY_PAUSE;
#if BB_INFO_HOLD_CHAR
    if (key.UnicodeChar == (CHAR16)BB_INFO_HOLD_CHAR)
        return BB_KEY_PAUSE;        /* extra "hold" key from the config */
#endif
    if (key.UnicodeChar == 0x000D)
        return BB_KEY_ENTER;
    return BB_KEY_OTHER;
}

static void c_sleep_ms(unsigned ms)
{
    g_st->BootServices->Stall((UINTN)ms * 1000u);
}

static void c_set_attr(unsigned fg, unsigned bg)
{
    /* EFI text attribute: low nibble foreground, next nibble background. */
    g_st->ConOut->SetAttribute(g_st->ConOut,
                               (UINTN)((fg & 0xFu) | ((bg & 0x7u) << 4)));
}

static const struct bb_console g_console = {
    c_clear, c_put, c_clear_line, c_rows, c_poll_key, c_sleep_ms, c_set_attr
};

const struct bb_console *uefi_console_init(EFI_SYSTEM_TABLE *st)
{
    UINTN cols, rows;

    g_st = st;
    if (st->ConOut->QueryMode(st->ConOut, (UINTN)st->ConOut->Mode->Mode,
                              &cols, &rows) == EFI_SUCCESS) {
        g_cols = cols;
        g_rows = rows;
    }
    st->ConOut->EnableCursor(st->ConOut, 0);
    return &g_console;
}

#ifdef BB_LOG_ENABLED
void uefi_log_status(const char *what, EFI_STATUS status)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[64];
    size_t n = 0;
    int shift;

    while (*what != '\0' && n < 40)
        buf[n++] = *what++;
    buf[n++] = ' ';
    buf[n++] = '0';
    buf[n++] = 'x';
    for (shift = (int)(sizeof(status) * 8) - 4; shift >= 0; shift -= 4)
        buf[n++] = hex[(status >> shift) & 0xF];
    buf[n] = '\0';
    BB_LOG(buf);
}
#endif
