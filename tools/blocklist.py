#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
"""Prints a BinBoot sector list (struct bb_extent[]) for a file.

usage: blocklist.py [--name NAME] FILE

For Legacy BIOS Linux entries (see struct bb_bios_linux). The file must sit
on a plain (non-LVM, non-RAID) partition of the disk BinBoot boots from, on
a filesystem that supports FIEMAP (ext4, xfs, btrfs on one device, ...).
Needs root to read the partition offset from sysfs on some systems.

IMPORTANT: the printed sectors are only valid while the file does not move.
Re-run this tool and rebuild BinBoot after every kernel/initrd update.
"""
import argparse
import fcntl
import os
import struct
import sys

SECTOR = 512
FS_IOC_FIEMAP = 0xC020660B
FIEMAP_FLAG_SYNC = 1
HDR = struct.Struct("=QQIIII")            # start, length, flags, mapped, count, rsvd
EXT = struct.Struct("=QQQQQIIII")         # logical, physical, length, rsvd x2, flags, rsvd x3
BAD_FLAGS = {
    0x02: "location unknown", 0x04: "delayed allocation",
    0x08: "compressed/encoded", 0x80: "encrypted", 0x200: "inline data",
    0x400: "tail-packed",
}
EXT_LAST = 0x01


def partition_start_sectors(dev):
    """Start of the partition holding device number `dev`, in sectors."""
    node = "/sys/dev/block/%d:%d" % (os.major(dev), os.minor(dev))
    if os.path.exists(node + "/dm") or os.path.exists(node + "/md"):
        sys.exit("error: the file is on LVM/device-mapper/RAID; "
                 "raw sector lists cannot describe that")
    try:
        with open(node + "/start") as f:
            return int(f.read())
    except FileNotFoundError:
        return 0                            # a whole disk, not a partition


def get_extents(fd, size):
    out, start = [], 0
    while True:
        count = 64
        buf = bytearray(HDR.size + count * EXT.size)
        HDR.pack_into(buf, 0, start, 0xFFFFFFFFFFFFFFFF - start,
                      FIEMAP_FLAG_SYNC, 0, count, 0)
        fcntl.ioctl(fd, FS_IOC_FIEMAP, buf, True)
        mapped = HDR.unpack_from(buf, 0)[3]
        if mapped == 0:
            break
        last = False
        for i in range(mapped):
            (logical, physical, length, _, _, flags, *_r) = EXT.unpack_from(
                buf, HDR.size + i * EXT.size)
            for bit, why in BAD_FLAGS.items():
                if flags & bit:
                    sys.exit("error: unsupported extent (%s)" % why)
            out.append((logical, physical, length))
            start = logical + length
            last = bool(flags & EXT_LAST)
        if last:
            break
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file")
    ap.add_argument("--name", default=None, help="C identifier prefix")
    args = ap.parse_args()

    size = os.stat(args.file).st_size
    name = args.name or os.path.basename(args.file).replace("-", "_").replace(".", "_")
    fd = os.open(args.file, os.O_RDONLY)
    try:
        extents = get_extents(fd, size)
    except OSError as e:
        sys.exit("error: FIEMAP failed (%s); filesystem not supported?" % e)
    if not extents:
        sys.exit("error: the file has no allocated blocks")
    base = partition_start_sectors(os.fstat(fd).st_dev)

    # Convert to absolute LBAs and merge neighbours that are contiguous.
    runs, expect_logical = [], 0
    for logical, physical, length in extents:
        if logical != expect_logical:
            sys.exit("error: the file has holes (sparse file)")
        expect_logical = logical + length
        if physical % SECTOR or length % SECTOR:
            sys.exit("error: extent is not sector aligned")
        lba, sectors = base + physical // SECTOR, length // SECTOR
        if lba + sectors >= 1 << 32:
            sys.exit("error: LBA does not fit in 32 bits (disk larger than 2 TiB)")
        if runs and runs[-1][0] + runs[-1][1] == lba:
            runs[-1][1] += sectors
        else:
            runs.append([lba, sectors])

    print("/* %s: %d bytes, %d extent(s), partition starts at LBA %d */"
          % (args.file, size, len(runs), base))
    print("static const struct bb_extent %s_ext[] = {" % name)
    for lba, sectors in runs:
        print("    { %d, %d }," % (lba, sectors))
    print("};")
    print("/* exact size in bytes (for .initrd_size): %d */" % size)
    print("/* verify (as root, DISK = the whole disk, e.g. /dev/sda):")
    print(" *   for each { lba, n }: dd if=DISK bs=512 skip=lba count=n | head -c SIZE | cmp - %s"
          % args.file)
    print(" * for a single extent SIZE is the file size; the sectors must match. */")


if __name__ == "__main__":
    main()
