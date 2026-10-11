#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
"""Creates a disk image with one FAT32 partition, as GPT or MBR.

usage: mkdisk.py gpt|mbr OUT.img     (prints the PARTUUID of partition 1)
The partition starts at LBA 2048 and is 128 MiB; the image file is sparse.
"""
import struct
import sys
import uuid
import zlib

SECTOR = 512
START, SIZE = 2048, 262144                  # 128 MiB
TOTAL = START + SIZE + 2048
kind, out = sys.argv[1:3]

img = open(out, "wb")
img.truncate(TOTAL * SECTOR)

if kind == "gpt":
    part_guid = uuid.UUID("6f1c2a3b-4d5e-4f60-8a7b-9c0d1e2f3a4b")
    disk_guid = uuid.UUID("0a1b2c3d-1111-2222-3333-444455556666")
    esp = uuid.UUID("c12a7328-f81f-11d2-ba4b-00a0c93ec93b")

    pmbr = bytearray(SECTOR)
    pmbr[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xEE,
                                b"\xff\xff\xff", 1, TOTAL - 1)
    pmbr[510:512] = b"\x55\xaa"

    entry = bytearray(128)
    entry[0:16] = esp.bytes_le
    entry[16:32] = part_guid.bytes_le
    struct.pack_into("<QQQ", entry, 32, START, START + SIZE - 1, 0)
    name = "BinBoot test".encode("utf-16-le")
    entry[56:56 + len(name)] = name                 # same length: no resize
    table = bytes(entry) + bytes(127 * 128)         # 128 entries
    table_crc = zlib.crc32(table) & 0xFFFFFFFF

    def header(cur, backup, entries_lba):
        h = bytearray(92)
        h[0:8] = b"EFI PART"
        struct.pack_into("<IIIIQQQQ", h, 8, 0x00010000, 92, 0, 0, cur, backup,
                         34, TOTAL - 34)
        h[56:72] = disk_guid.bytes_le
        struct.pack_into("<QIII", h, 72, entries_lba, 128, 128, table_crc)
        struct.pack_into("<I", h, 16, zlib.crc32(bytes(h)) & 0xFFFFFFFF)
        return bytes(h).ljust(SECTOR, b"\0")

    img.seek(0);                 img.write(pmbr)
    img.seek(1 * SECTOR);        img.write(header(1, TOTAL - 1, 2))
    img.seek(2 * SECTOR);        img.write(table)
    img.seek((TOTAL - 33) * SECTOR); img.write(table)
    img.seek((TOTAL - 1) * SECTOR);  img.write(header(TOTAL - 1, 1, TOTAL - 33))
    print(str(part_guid))
else:
    sig = 0x12345678
    mbr = bytearray(SECTOR)
    struct.pack_into("<I", mbr, 440, sig)
    mbr[446:462] = struct.pack("<B3sB3sII", 0x80, b"\xfe\xff\xff", 0x0C,
                               b"\xfe\xff\xff", START, SIZE)
    mbr[510:512] = b"\x55\xaa"
    img.seek(0); img.write(mbr)
    print("%08x-01" % sig)
img.close()
