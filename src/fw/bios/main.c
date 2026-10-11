/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "binboot.h"
#include "bios.h"
#include "config.h"
#include "disk.h"
#include "error.h"
#include "linux.h"
#include "log.h"
#include "menu.h"

unsigned char bb_boot_drive;

#define VBR_ADDR 0x7C00u
#define MBR_COPY 0x0600u    /* Classic place for the MBR copy DOS/NT expect */

#ifdef BB_LOG_ENABLED
static void bios_log_sink(const char *msg)
{
    bios_serial_write(msg);
    bios_serial_write("\r\n");
}
#endif

/* Single-sector read into a buffer below 64 KiB (segment 0). */
static unsigned disk_read(unsigned char drive, unsigned lba, void *dest)
{
    return bb_disk_read(drive, lba, 1, 0, (unsigned)dest);
}

static unsigned le32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static int has_boot_signature(const unsigned char *sector)
{
    return sector[510] == 0x55 && sector[511] == 0xAA;
}

__attribute__((noreturn))
static void jump_to_vbr(unsigned char drive, unsigned short si)
{
    /* Same handover as a BIOS gives: DL = drive, DS:SI = partition entry,
     * segments zero, execution at 0000:7C00. */
    __asm__ volatile(
        "cli\n\t"
        "xorw %%ax, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "movw $0x7C00, %%sp\n\t"
        "sti\n\t"
        "ljmp $0x0000, $0x7C00"
        : : "d"((unsigned)drive), "S"((unsigned)si)
        : "memory");
    __builtin_unreachable();
}

/* Chainloads a boot sector. Returns only on failure, with the error set. */
static void chain_boot(const struct bb_bios_chain *c)
{
    unsigned char drive = (c->drive == BB_BIOS_BOOT_DRIVE) ? bb_boot_drive
                                                           : c->drive;
    unsigned char *mbr = (unsigned char *)MBR_COPY;
    const unsigned char *vbr = (const unsigned char *)VBR_ADDR;
    unsigned lba = 0, err;
    unsigned short si = (unsigned short)(VBR_ADDR + 446u);

    if (c->partition > 4) {
        bb_error_set("Invalid partition number (must be 0 to 4)", 0,
                     BB_ERR_NOCODE);
        return;
    }

    if (c->partition != 0) {
        const unsigned char *e;

        err = disk_read(drive, 0, mbr);
        if (err != 0) {
            bb_error_set("Cannot read the disk (INT 13h)", 0, err);
            return;
        }
        if (!has_boot_signature(mbr)) {
            bb_error_set("The disk has no valid MBR (missing 55AA)", 0,
                         BB_ERR_NOCODE);
            return;
        }
        e = mbr + 446 + 16u * (c->partition - 1u);
        lba = le32(e + 8);
        if (e[4] == 0 || lba == 0) {
            bb_error_set("The partition is empty or does not exist", 0,
                         BB_ERR_NOCODE);
            return;
        }
        si = (unsigned short)(unsigned)e;
    }

    err = disk_read(drive, lba, (void *)VBR_ADDR);
    if (err != 0) {
        bb_error_set("Cannot read the boot sector (INT 13h)", 0, err);
        return;
    }
    if (!has_boot_signature(vbr)) {
        bb_error_set("The boot sector is not valid (missing 55AA)", 0,
                     BB_ERR_NOCODE);
        return;
    }

    BB_LOG("BinBoot: jumping to boot sector");
    bios_video_restore();
    jump_to_vbr(drive, si);
}

void bb_bios_main(void)
{
    const struct bb_console *con;
    int autoboot = 1;

#ifdef BB_LOG_ENABLED
    bios_serial_init();
#endif
    BB_LOG_SET_SINK(bios_log_sink);
    BB_LOG("BinBoot: BIOS started");

    con = bios_console_init();
    for (;;) {
        size_t chosen = bb_menu_run(con, autoboot, 0);

        if (chosen == BB_MENU_NONE) {
            bb_error_set("No boot entries are configured", 0, BB_ERR_NOCODE);
            bb_error_screen(con, 0);
            continue;
        }
        if (bb_entries[chosen].chain.enabled)
            chain_boot(&bb_entries[chosen].chain);
        else if (bb_entries[chosen].bios_linux.enabled)
            bb_bios_linux_boot(&bb_entries[chosen].bios_linux,
                               bb_entries[chosen].cmdline);
        else
            bb_error_set("This entry cannot be booted in BIOS mode", 0,
                         BB_ERR_NOCODE);

        /* Only reached on failure: show it until Enter, then back to the
         * menu without a timeout. */
        BB_LOG("BinBoot: boot failed");
        bb_error_screen(con, bb_entries[chosen].name);
        autoboot = 0;
    }
}
