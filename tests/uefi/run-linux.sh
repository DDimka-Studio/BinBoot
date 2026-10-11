#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# Boots a real EFI-stub Linux kernel through BinBoot (x86_64, QEMU+OVMF).
# Usage: tests/uefi/run-linux.sh /path/to/vmlinuz
set -eu
cd "$(dirname "$0")/../.."
KERNEL="${1:?usage: run-linux.sh /path/to/vmlinuz}"

W=build/uefi-linux-test
rm -rf "$W"; mkdir -p "$W/root" "$W/esp/EFI/linux"
gcc -static -O2 -o "$W/root/init" tests/uefi/init.c
python3 tests/uefi/mkinitrd.py "$W/root/init" "$W/esp/EFI/linux/initrd.img"
cp "$KERNEL" "$W/esp/EFI/linux/vmlinuz"

make BUILD=debug CONFIG_C=tests/uefi/config_linux.c >/dev/null
timeout 240 tools/run-qemu.sh x86_64 \
    build/x86_64/debug-custom/BOOTX64.EFI "$W/esp" </dev/null > "$W/serial.log" 2>&1 || true

python3 - "$W/serial.log" <<'PY'
import re, sys
t = open(sys.argv[1], "rb").read().decode("latin-1").replace("\x1b", "")
for pat in (r"BinBoot: [^\r\n]*", r"INITRD-OK", r"CMDLINE: [^\r\n]*",
            r"Run /init as init process", r"Linux version [^\r\n]{0,40}"):
    for m in re.findall(pat, t):
        print(m)
PY
