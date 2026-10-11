/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * BinBoot boot entries: what the menu offers.
 *
 * HOW THIS FILE WORKS
 *   - There is one list per firmware: section 1 for UEFI, section 2 for
 *     Legacy BIOS. Only the list for the firmware you build for is used.
 *   - An entry is one { ... } block. The menu shows them in this order, and
 *     BB_DEFAULT_ENTRY (config.h) counts from 0 along it.
 *   - ADD an entry: copy a block from the TEMPLATES below into the list,
 *     put a comma after its closing brace, change the values.
 *   - REMOVE an entry: delete its { ... } block, or wrap it in a comment.
 *   - REORDER: move the blocks (and mind BB_DEFAULT_ENTRY).
 *   - Strings are plain ASCII. Anything you leave out is "not set".
 * Then rebuild. Field details: docs/configuration.md.
 */
#include "binboot.h"
#include "config.h"

#if defined(BB_FW_BIOS)
/* ======================================================================
 * 2. Legacy BIOS entries
 *
 * THE FIRST ENTRY IS GENERATED. `make build_bios part=N` makes partition N
 * (1..4) of the boot disk the first entry and therefore the one that boots
 * automatically; without part= it is partition 1. Everything you add below
 * simply follows it, in the order written. To drop the generated entry,
 * delete its block (then part= has no effect).
 *
 * TEMPLATES (copy one into the list below the generated entry)
 *
 *   Start the boot sector of another partition (Windows, BSD, another loader):
 *     {
 *         .name  = "Windows",
 *         .chain = { .enabled = 1,
 *                    .drive = BB_BIOS_BOOT_DRIVE,   // or 0x80, 0x81, ...
 *                    .partition = 2 },              // 1..4; 0 = disk's MBR
 *     },
 *
 *   Start Linux from raw disk sectors (no filesystem needed). Get the
 *   sector lists with `sudo tools/blocklist.py FILE --name kernel`:
 *     static const struct bb_extent kernel_ext[] = { { 20480, 29429 } };
 *     static const struct bb_extent initrd_ext[] = { { 51200, 665 } };
 *     ...
 *     {
 *         .name    = "Linux",
 *         .cmdline = "root=/dev/sda2 rw quiet",
 *         .bios_linux = {
 *             .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE,
 *             .kernel = kernel_ext, .kernel_count = BB_COUNT(kernel_ext),
 *             .initrd = initrd_ext, .initrd_count = BB_COUNT(initrd_ext),
 *             .initrd_size = 339000,     // exact bytes, from blocklist.py
 *         },
 *     },
 * ====================================================================== */

#ifndef BB_FIRST_PART
#define BB_FIRST_PART 1             /* set by `make build_bios part=N` */
#endif
#define BB_STR_(x) #x
#define BB_STR(x)  BB_STR_(x)

const struct bb_entry bb_entries[] = {
    /* Generated first entry: partition BB_FIRST_PART of the boot disk. */
    {
        .name  = BB_PART_ENTRY_PREFIX BB_STR(BB_FIRST_PART),
        .chain = { .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE,
                   .partition = BB_FIRST_PART },
    },
    /* Your entries go here. */
};

#else
/* ======================================================================
 * 1. UEFI entries
 *
 * TEMPLATES (copy one into the list below)
 *
 *   A. An EFI program from a file: a UKI, Windows, another boot manager.
 *      .volume picks the disk partition by PARTUUID (`lsblk -o NAME,PARTUUID`):
 *      a GPT GUID, or "XXXXXXXX-NN" on an MBR disk. Leave it out to use the
 *      volume BinBoot itself was loaded from.
 *     {
 *         .name     = "Arch (UKI)",
 *         .uki_path = "/EFI/Linux/arch.efi",
 *         .volume   = "6f1c2a3b-4d5e-4f60-8a7b-9c0d1e2f3a4b",
 *     },
 *     {
 *         .name     = "Windows",
 *         .uki_path = "/EFI/Microsoft/Boot/bootmgfw.efi",
 *     },
 *
 *   B. A Linux kernel with an EFI stub, plus initrd and command line, read
 *      from a disk (same .volume rule as above):
 *     {
 *         .name    = "Linux",
 *         .kernel  = "/EFI/linux/vmlinuz",
 *         .initrd  = "/EFI/linux/initrd.img",
 *         .cmdline = "root=/dev/sda2 rw quiet",
 *     },
 *
 *   C. An EFI program stored INSIDE the BinBoot file (the file then needs
 *      nothing else on disk). Put the file under blobs/<arch>/ and name it
 *      once at the top of this section:
 *        BB_EMBED(blob_rescue, "blobs/" BB_ARCH "/rescue.efi");
 *     {
 *         .name  = "Rescue",
 *         .image = &blob_rescue,
 *     },
 *
 * Paths start at the root of the chosen volume: if that partition is
 * mounted at /boot on your system, a file /boot/vmlinuz is "/vmlinuz" here.
 * ====================================================================== */

const struct bb_entry bb_entries[] = {
    {
        .name     = "Linux (UKI)",
        .uki_path = "/EFI/Linux/linux.efi",
        /* .volume = "6f1c2a3b-4d5e-4f60-8a7b-9c0d1e2f3a4b",   GPT disk */
        /* .volume = "12345678-01",                            MBR disk */
    },
};

#endif

/* Do not edit: number of entries above. */
const size_t bb_entry_count = sizeof(bb_entries) / sizeof(bb_entries[0]);
