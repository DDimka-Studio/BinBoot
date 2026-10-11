#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
"""Builds a gzip-compressed 'newc' initramfs holding /proc and /init.

usage: mkinitrd.py INIT_BINARY OUT.img     (no cpio tool needed)
"""
import gzip
import sys

def pad4(n):
    return (4 - n % 4) % 4

def entry(name, mode, data=b"", ino=1):
    nm = name.encode() + b"\0"
    fields = [ino, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(nm), 0]
    hdr = b"070701" + b"".join(b"%08X" % f for f in fields)
    blob = hdr + nm + b"\0" * pad4(len(hdr) + len(nm))
    return blob + data + b"\0" * pad4(len(data))

init = open(sys.argv[1], "rb").read()
archive = (entry("proc", 0o040755, ino=1) +
           entry("init", 0o100755, init, ino=2) +
           entry("TRAILER!!!", 0, ino=0))
with gzip.open(sys.argv[2], "wb", 9) as f:
    f.write(archive)
