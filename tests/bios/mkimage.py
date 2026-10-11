#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
"""Creates a 16 MiB MBR test disk with two partitions that hold test VBRs.

usage: mkimage.py OUT.img VBR1.bin VBR2.bin
"""
import struct
import sys

SECTOR = 512
out, vbr1, vbr2 = sys.argv[1:4]
P1_START, P1_SIZE = 2048, 8192
P2_START, P2_SIZE = 10240, 8192

def entry(boot, ptype, start, size):
    # CHS fields are unused by LBA-aware code; use the 'max' marker.
    return struct.pack("<B3sB3sII", boot, b"\xfe\xff\xff", ptype,
                       b"\xfe\xff\xff", start, size)

img = bytearray(16 * 1024 * 1024)
mbr = bytearray(SECTOR)
struct.pack_into("<I", mbr, 440, 0xB1B0B007)      # disk signature
mbr[446:462] = entry(0x80, 0x83, P1_START, P1_SIZE)
mbr[462:478] = entry(0x00, 0x83, P2_START, P2_SIZE)
mbr[510:512] = b"\x55\xaa"
img[0:SECTOR] = mbr
for start, path in ((P1_START, vbr1), (P2_START, vbr2)):
    data = open(path, "rb").read()
    assert len(data) == SECTOR, path
    img[start * SECTOR:(start + 1) * SECTOR] = data
open(out, "wb").write(img)
