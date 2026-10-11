#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# Boots a real bzImage from raw sectors through the Legacy BIOS loader.
# Usage: tests/bios/run-linux.sh /path/to/vmlinuz
set -eu
cd "$(dirname "$0")/../.."
KERNEL="${1:?usage: run-linux.sh /path/to/vmlinuz}"

W=build/bios-linux-test
rm -rf "$W"; mkdir -p "$W/root"
gcc -static -O2 -o "$W/root/init" tests/uefi/init.c
python3 tests/uefi/mkinitrd.py "$W/root/init" "$W/initrd.img"
python3 tests/bios/mkimage_linux.py "$KERNEL" "$W/initrd.img" "$W/disk.img" "$W/config.c"

make bios BUILD=debug CONFIG_C="$W/config.c" >/dev/null
B=build/bios/debug-custom
tools/install-bios.sh --yes --build "$B" --backup "$W/mbr.bak" "$W/disk.img" >/dev/null

run_qemu() {
    qemu-system-x86_64 -m 512 -display none -no-reboot \
        -serial "file:$W/$1.serial" -monitor stdio \
        -drive "file=$W/disk.img,format=raw,if=ide" >/dev/null 2>&1
}

echo "--- test 1: Linux from raw sectors (autoboot) ---"
( sleep 100; echo quit ) | timeout 150 qemu-system-x86_64 -m 512 -display none -no-reboot \
    -serial "file:$W/t1.serial" -monitor stdio \
    -drive "file=$W/disk.img,format=raw,if=ide" >/dev/null 2>&1 || true
python3 - "$W/t1.serial" <<'PY'
import re, sys
t = open(sys.argv[1], "rb").read().decode("latin-1")
for pat in (r"BinBoot: [^\r\n]*", r"Linux version [^\r\n]{0,30}", r"Command line: [^\r\n]*",
            r"Run /init as init process", r"INITRD-OK", r"CMDLINE: [^\r\n]*"):
    for m in dict.fromkeys(re.findall(pat, t)):
        print(m)
PY

echo "--- test 2: invalid kernel shows an error until Enter ---"
( sleep 3; echo "sendkey down"; sleep 1; echo "sendkey ret"; sleep 3
  echo "pmemsave 0xb8000 4000 $W/t2a.vga"; echo "sendkey ret"; sleep 2
  echo "pmemsave 0xb8000 4000 $W/t2b.vga"; echo quit ) | run_qemu t2
echo "error screen:";  python3 tests/bios/vga.py "$W/t2a.vga"
echo "after Enter:";   python3 tests/bios/vga.py "$W/t2b.vga"
