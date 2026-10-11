#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
"""Rebuilds the final text screen from a QEMU serial log (ANSI cursor codes)
and prints its non-empty rows. usage: screen.py LOG"""
import re
import sys

data = open(sys.argv[1], "rb").read().decode("latin-1")
W, H = 240, 80
grid = [[" "] * W for _ in range(H)]
r = c = 0
i = 0
while i < len(data):
    ch = data[i]
    if ch == "\x1b" and i + 1 < len(data) and data[i + 1] == "[":
        m = re.match(r"\x1b\[([0-9;?]*)([A-Za-z])", data[i:])
        if m:
            params, final = m.groups()
            if final == "H":
                nums = [int(x) if x else 1 for x in params.split(";")] if params else [1, 1]
                r, c = nums[0] - 1, (nums[1] - 1 if len(nums) > 1 else 0)
            elif final == "J" and params in ("2", "0", ""):
                if params == "2":
                    grid = [[" "] * W for _ in range(H)]
            i += len(m.group(0))
            continue
    if ch == "\r":
        c = 0
    elif ch == "\n":
        r += 1
    elif ch >= " " and 0 <= r < H and 0 <= c < W:
        grid[r][c] = ch
        c += 1
    i += 1
for row in grid:
    line = "".join(row).rstrip()
    if line:
        print("  |" + line)
