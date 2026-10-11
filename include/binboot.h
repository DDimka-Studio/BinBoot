/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* BinBoot public types shared by core code and the user configuration. */
#ifndef BINBOOT_H
#define BINBOOT_H

#include <stddef.h>

/* An embedded file: 64-bit size followed immediately by the raw bytes. */
struct bb_blob {
    unsigned long long size;
    unsigned char data[];
};

/*
 * BB_EMBED(symbol, "path") links the file at "path" (relative to the
 * project root) into the binary and declares: extern const struct bb_blob
 * symbol. The assembler computes the size, so no runtime or build-script
 * step is needed.
 */
#ifndef BB_SYM_PREFIX
#define BB_SYM_PREFIX ""            /* IA32 COFF decorates C names with "_" */
#endif
#define BB_EMBED(sym, path)                                          \
    extern const struct bb_blob sym;                                 \
    __asm__(".pushsection .rdata,\"dr\"\n"                          \
            ".balign 16\n"                                           \
            ".globl " BB_SYM_PREFIX #sym "\n"                        \
            BB_SYM_PREFIX #sym ":\n"                                 \
            ".quad " #sym "_end - " #sym "_data\n"                   \
            #sym "_data:\n"                                          \
            ".incbin \"" path "\"\n"                                 \
            #sym "_end:\n"                                           \
            ".popsection\n")

/* Legacy BIOS only: chainload the boot sector of a disk or partition. */
#define BB_BIOS_BOOT_DRIVE 0xFF   /* "the drive BinBoot itself booted from" */
struct bb_bios_chain {
    unsigned char enabled;    /* 0 = unused, 1 = chainload */
    unsigned char drive;      /* BIOS drive number (0x80 = first disk) or
                                 BB_BIOS_BOOT_DRIVE */
    unsigned char partition;  /* 1..4 = primary partition (MBR); 0 = the
                                 disk's own MBR */
};

/* A run of consecutive disk sectors (LBA, 512-byte sectors). */
struct bb_extent {
    unsigned lba;
    unsigned sectors;
};
#define BB_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/*
 * Legacy BIOS only: boot a Linux bzImage whose kernel and initrd are given
 * as raw sector lists, so no filesystem driver is needed (like LILO).
 * tools/blocklist.py prints these lists for a file on a Linux filesystem.
 * The kernel command line is the entry's .cmdline.
 */
struct bb_bios_linux {
    unsigned char enabled;
    unsigned char drive;                /* BIOS drive or BB_BIOS_BOOT_DRIVE */
    const struct bb_extent *kernel;
    unsigned kernel_count;
    const struct bb_extent *initrd;     /* optional: count 0 = no initrd */
    unsigned initrd_count;
    unsigned initrd_size;               /* exact bytes; 0 = whole sectors */
};

/* One menu entry. All strings are plain ASCII and live in read-only data. */
struct bb_entry {
    const char *name;     /* Text shown in the menu */
    const char *kernel;   /* Path to the kernel image */
    const char *initrd;   /* Path to initrd/initramfs, or NULL */
    const char *cmdline;  /* Kernel command line, or NULL */
    /* UEFI: boot an EFI binary / UKI from a file on a volume (path from
     * the volume root, e.g. "/EFI/Linux/linux.efi"). */
    const char *uki_path;
    /* UEFI: the volume that .uki_path, .kernel and .initrd are read from,
     * as a Linux-style PARTUUID: a GPT partition GUID
     * ("6f1c2a3b-4d5e-4f60-8a7b-9c0d1e2f3a4b") or "XXXXXXXX-NN" for MBR
     * disks (disk signature, partition number). NULL = the volume BinBoot
     * itself was loaded from. `blkid` / `lsblk -o PARTUUID` show it. */
    const char *volume;
    /* Embedded EFI image (e.g. a UKI). If set, it is booted directly and
     * kernel/initrd/cmdline are ignored. */
    const struct bb_blob *image;
    /* Legacy BIOS only (ignored by UEFI builds): chainload a boot sector,
     * or boot Linux from raw sectors. */
    struct bb_bios_chain chain;
    struct bb_bios_linux bios_linux;
};

/* Defined by the user in config/config.c */
extern const struct bb_entry bb_entries[];
extern const size_t bb_entry_count;

#endif /* BINBOOT_H */
