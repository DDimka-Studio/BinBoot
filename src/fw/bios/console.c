/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "bios.h"

#define COLS 80u
#define ROWS 25u
#define VGA_SEG 0xB800u
static unsigned g_attr = 0x07u;     /* current VGA attribute byte */
#define ATTR_NORMAL g_attr

/* Writes one text cell straight into video memory (segment B800). */
static void vga_poke(unsigned off, unsigned cell)
{
    __asm__ volatile(
        "pushw %%es\n\t"
        "movw %0, %%es\n\t"
        "movw %2, %%es:(%1)\n\t"
        "popw %%es"
        : : "r"((unsigned short)VGA_SEG), "r"(off),
            "r"((unsigned short)cell)
        : "memory");
}

static void int10(unsigned ax, unsigned cx)
{
    /* BIOS video calls may trash several registers, BP included. */
    __asm__ volatile(
        "pushl %%ebp\n\t"
        "int $0x10\n\t"
        "popl %%ebp"
        : "+a"(ax), "+c"(cx)
        : : "ebx", "edx", "esi", "edi", "cc", "memory");
}

void bios_video_restore(void)
{
    int10(0x0003, 0);               /* 80x25 colour text, clears screen */
}

static void c_clear(void)
{
    unsigned i;

    for (i = 0; i < COLS * ROWS; i++)
        vga_poke(i * 2u, (ATTR_NORMAL << 8) | ' ');
}

static void c_put(unsigned col, unsigned row, const char *s)
{
    unsigned off;

    if (row >= ROWS)
        return;
    for (; *s != '\0' && col < COLS; s++, col++) {
        off = (row * COLS + col) * 2u;
        vga_poke(off, (ATTR_NORMAL << 8) | (unsigned char)*s);
    }
}

static void c_clear_line(unsigned row)
{
    unsigned col;

    if (row >= ROWS)
        return;
    for (col = 0; col < COLS; col++)
        vga_poke((row * COLS + col) * 2u, (ATTR_NORMAL << 8) | ' ');
}

static unsigned c_rows(void)
{
    return ROWS;
}

static enum bb_key c_poll_key(void)
{
    unsigned ax, none;

    /* AH=01h peeks (ZF=1: no key); AH=00h then takes the key. */
    __asm__ volatile(
        "pushl %%ebp\n\t"
        "movb $0x01, %%ah\n\t"
        "int $0x16\n\t"
        "jz 1f\n\t"
        "xorl %%eax, %%eax\n\t"
        "int $0x16\n\t"
        "movzwl %%ax, %%eax\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "jmp 2f\n"
        "1:\n\t"
        "movl $1, %%ecx\n"
        "2:\n\t"
        "popl %%ebp"
        : "=a"(ax), "=c"(none)
        : : "ebx", "edx", "esi", "edi", "cc", "memory");

    if (none)
        return BB_KEY_NONE;
    if ((ax >> 8) == 0x48u)
        return BB_KEY_UP;
    if ((ax >> 8) == 0x50u)
        return BB_KEY_DOWN;
    if ((ax & 0xFFu) == 0x0Du)
        return BB_KEY_ENTER;
    return BB_KEY_OTHER;
}

static void c_sleep_ms(unsigned ms)
{
    unsigned us = ms * 1000u;
    unsigned cx = us >> 16, dx = us & 0xFFFFu;

    /* INT 15h AH=86h: wait CX:DX microseconds. */
    __asm__ volatile(
        "pushl %%ebp\n\t"
        "movb $0x86, %%ah\n\t"
        "int $0x15\n\t"
        "popl %%ebp"
        : "+c"(cx), "+d"(dx)
        : : "eax", "ebx", "esi", "edi", "cc", "memory");
}

static void c_set_attr(unsigned fg, unsigned bg)
{
    g_attr = (fg & 0xFu) | ((bg & 0x7u) << 4);
}

static const struct bb_console g_console = {
    c_clear, c_put, c_clear_line, c_rows, c_poll_key, c_sleep_ms, c_set_attr
};

const struct bb_console *bios_console_init(void)
{
    bios_video_restore();
    int10(0x0100, 0x2000);          /* hide the hardware cursor */
    return &g_console;
}

#ifdef BB_LOG_ENABLED
/* Debug log goes to COM1 so it can be captured without a screen. */
static void outb(unsigned short port, unsigned char v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static unsigned char inb(unsigned short port)
{
    unsigned char v;

    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

void bios_serial_init(void)
{
    outb(0x3F9, 0x00);              /* no interrupts */
    outb(0x3FB, 0x80);              /* DLAB on */
    outb(0x3F8, 0x01);              /* divisor 1 = 115200 baud */
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);              /* 8N1, DLAB off */
    outb(0x3FA, 0xC7);              /* enable FIFO */
    outb(0x3FC, 0x0B);
}

void bios_serial_write(const char *s)
{
    unsigned spin;

    for (; *s != '\0'; s++) {
        /* Bounded wait so a missing UART cannot hang the boot. */
        for (spin = 0; spin < 20000u && !(inb(0x3FD) & 0x20); spin++)
            ;
        outb(0x3F8, (unsigned char)*s);
    }
}
#endif
