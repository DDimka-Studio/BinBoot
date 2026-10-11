#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
"""Prints the non-empty lines of a raw 80x25 VGA text dump."""
import sys
data = open(sys.argv[1], "rb").read()
for row in range(25):
    line = bytes(data[(row * 80 + c) * 2] for c in range(80)).decode("latin-1")
    if line.strip():
        print("  |" + line.rstrip())
