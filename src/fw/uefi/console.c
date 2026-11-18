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

static void c_clear_line(unsigned row);

static void c_clear(void)
{
    unsigned r;

    g_st->ConOut->ClearScreen(g_st->ConOut);
    /* Some firmwares leave text behind after ClearScreen, most often after a
     * text-mode change. Blanking every row explicitly removes it. */
    for (r = 0; r < g_rows; r++)
        c_clear_line(r);
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
    /* An empty report (no character, no scan code) is not a key press.
     * Some firmwares send them, and they must not cancel the countdown. */
    if (key.ScanCode == 0 && key.UnicodeChar == 0)
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

static const EFI_GUID guid_gop = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

#define STABLE_SAMPLE_MS 100u   /* pause between resolution readings */
#define STABLE_MAX_TRIES 15u    /* give up after about 1.5 s */

/* One reading of the display's current resolution. 0 if there is no GOP. */
static int read_display(EFI_SYSTEM_TABLE *st, uint32_t *w, uint32_t *h)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;

    if (st->BootServices->LocateProtocol(&guid_gop, 0, (void **)&gop) !=
            EFI_SUCCESS || gop == 0 || gop->Mode == 0 || gop->Mode->Info == 0)
        return 0;
    *w = gop->Mode->Info->HorizontalResolution;
    *h = gop->Mode->Info->VerticalResolution;
    return 1;
}

/* Reads the resolution until three readings in a row agree. Returns 0 if
 * there is no display information or it never settles: then the caller
 * uses a fixed mode instead of guessing. */
static int display_size(EFI_SYSTEM_TABLE *st, uint32_t *w, uint32_t *h)
{
    uint32_t w0, h0, w1, h1;
    unsigned tries, same = 0;

    if (!read_display(st, &w0, &h0))
        return 0;
    for (tries = 0; tries < STABLE_MAX_TRIES; tries++) {
        st->BootServices->Stall(STABLE_SAMPLE_MS * 1000u);
        if (!read_display(st, &w1, &h1))
            return 0;
        if (w1 == w0 && h1 == h0) {
            if (++same == 2) {          /* 3 readings: first + 2 equal */
                *w = w1;
                *h = h1;
                BB_LOG_STATUS("BinBoot: display width", w1);
                BB_LOG_STATUS("BinBoot: display height", h1);
                return 1;
            }
        } else {
            w0 = w1;
            h0 = h1;
            same = 0;
        }
    }
    BB_LOG("BinBoot: display resolution did not settle");
    return 0;
}

/* Text mode whose pixel shape (8x16 cells, the usual font) best matches the
 * display while still fitting on it. Mode 0 if nothing fits. */
static UINTN pick_text_mode(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,
                            uint32_t w, uint32_t h)
{
    UINTN best = 0, m, max = (UINTN)out->Mode->MaxMode;
    uint64_t best_err = 0, best_area = 0;
    int found = 0;

    for (m = 0; m < max; m++) {
        UINTN c, r;
        uint64_t pw, ph, err, area;

        if (out->QueryMode(out, m, &c, &r) != EFI_SUCCESS)
            continue;
        pw = (uint64_t)c * 8u;
        ph = (uint64_t)r * 16u;
        if (pw > w || ph > h)
            continue;                   /* does not fit on the display */
        /* |pw/ph - w/h| without division: compare cross products. */
        err = (pw * h > (uint64_t)w * ph) ? pw * h - (uint64_t)w * ph
                                          : (uint64_t)w * ph - pw * h;
        area = pw * ph;
        if (!found || err * best_area < best_err * area ||
            (err * best_area == best_err * area && area > best_area)) {
            best = m;
            best_err = err;
            best_area = area;
            found = 1;
        }
    }
    return best;
}

/* Largest resolution chosen automatically, so a 4K panel does not get text
 * that is too small to read. */
#define AUTO_MAX_W 1920u
#define AUTO_MAX_H 1200u

/* Sets the display resolution: the configured one exactly, or when none is
 * configured the largest offered one within AUTO_MAX. If nothing can be set,
 * the firmware's current mode stays (the fallback). */
static void set_display(EFI_SYSTEM_TABLE *st)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    uint32_t i, n, want_w = 0, want_h = 0, best = 0xFFFFFFFFu, best_area = 0;
    int explicit_size = 0;

#if defined(BB_DISPLAY_WIDTH) && defined(BB_DISPLAY_HEIGHT)
    want_w = BB_DISPLAY_WIDTH;
    want_h = BB_DISPLAY_HEIGHT;
    explicit_size = 1;
#endif

    if (st->BootServices->LocateProtocol(&guid_gop, 0, (void **)&gop) !=
            EFI_SUCCESS || gop == 0 || gop->Mode == 0)
        return;
    n = gop->Mode->MaxMode;
    for (i = 0; i < n; i++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = 0;
        UINTN size = 0;
        uint32_t w, h;

        if (gop->QueryMode(gop, i, &size, &info) != EFI_SUCCESS || info == 0)
            continue;
        w = info->HorizontalResolution;
        h = info->VerticalResolution;
        st->BootServices->FreePool(info);

        if (explicit_size) {
            if (w == want_w && h == want_h) {
                best = i;
                break;
            }
        } else if (w <= AUTO_MAX_W && h <= AUTO_MAX_H &&
                   (uint32_t)(w * h) > best_area) {
            best = i;
            best_area = w * h;
        }
    }

    if (best == 0xFFFFFFFFu) {
        BB_LOG(explicit_size ? "BinBoot: configured resolution not offered"
                             : "BinBoot: no usable resolution, keeping current");
        return;
    }
    if (best == gop->Mode->Mode)
        return;                         /* already the one we want */
    if (gop->SetMode(gop, best) != EFI_SUCCESS)
        BB_LOG("BinBoot: display refused the resolution, keeping current");
    else
        BB_LOG(explicit_size ? "BinBoot: display resolution from config"
                             : "BinBoot: display resolution chosen automatically");
}

const struct bb_console *uefi_console_init(EFI_SYSTEM_TABLE *st)
{
    UINTN mode, cols, rows;
    uint32_t w = 0, h = 0;

    g_st = st;
    set_display(st);
    /* Set the text mode once, here, and never again: the firmware's own
     * mode can differ between machines (and change), which moved the text. */
    if (BB_CONSOLE_MODE >= 0)
        mode = (UINTN)BB_CONSOLE_MODE;
    else if (display_size(st, &w, &h))
        mode = pick_text_mode(st->ConOut, w, h);
    else
        mode = 0;
    if (mode >= (UINTN)st->ConOut->Mode->MaxMode)
        mode = 0;
    if (st->ConOut->SetMode(st->ConOut, mode) != EFI_SUCCESS)
        mode = (UINTN)st->ConOut->Mode->Mode;   /* keep what there is */
    st->ConOut->ClearScreen(st->ConOut);
    if (st->ConOut->QueryMode(st->ConOut, mode, &cols, &rows) == EFI_SUCCESS) {
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
