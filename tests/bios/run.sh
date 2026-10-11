#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# End-to-end test of the Legacy BIOS target in QEMU (SeaBIOS).
# Checks: installer, menu, countdown, key navigation, chainloading.
set -eu
cd "$(dirname "$0")/../.."

W=build/bios/test
rm -rf "$W"; mkdir -p "$W"
make bios BUILD=debug CONFIG_C=tests/bios/config.c >/dev/null
B=build/bios/debug-custom

for n in 1 2; do
    clang -target i386-pc-none-elf -DMSG="\"VBR:PART$n\"" -c tests/bios/vbr.S -o "$W/vbr$n.o"
    ld.lld -m elf_i386 -T tests/bios/vbr.ld --oformat binary -o "$W/vbr$n.bin" "$W/vbr$n.o"
done
python3 tests/bios/mkimage.py "$W/disk.img" "$W/vbr1.bin" "$W/vbr2.bin"
tools/install-bios.sh --yes --build $B --backup "$W/mbr.bak" "$W/disk.img" >/dev/null
echo "installer: ok"

run_qemu() {  # $1 = log name, reads monitor commands from stdin
    qemu-system-i386 -m 64 -display none -no-reboot \
        -serial "file:$W/$1.serial" -monitor stdio \
        -drive "file=$W/disk.img,format=raw,if=ide" >/dev/null 2>&1
}

echo "--- test 1: autoboot default (Partition 1) ---"
( sleep 12; echo "pmemsave 0xb8000 4000 $W/t1.vga"; echo quit ) | run_qemu t1
grep -a -E "BIOS started|jumping|VBR" "$W/t1.serial" | tr -d '\r'

echo "--- test 2: Down, Enter (Partition 2); countdown cancelled ---"
( sleep 3; echo "pmemsave 0xb8000 4000 $W/t2a.vga"; echo "sendkey down"; sleep 1
  echo "pmemsave 0xb8000 4000 $W/t2b.vga"; echo "sendkey ret"; sleep 3
  echo quit ) | run_qemu t2
echo "screen before keys:"; python3 tests/bios/vga.py "$W/t2a.vga"
echo "screen after Down:";  python3 tests/bios/vga.py "$W/t2b.vga"
grep -a -E "jumping|VBR" "$W/t2.serial" | tr -d '\r'

echo "--- test 3: boot failure shows error until Enter, then menu ---"
( sleep 3; echo "sendkey down"; sleep 1; echo "sendkey down"; sleep 1
  echo "sendkey ret"; sleep 2
  echo "pmemsave 0xb8000 4000 $W/t3a.vga"
  echo "sendkey x"; sleep 1; echo "pmemsave 0xb8000 4000 $W/t3b.vga"
  echo "sendkey ret"; sleep 2; echo "pmemsave 0xb8000 4000 $W/t3c.vga"
  echo "sendkey up"; sleep 1; echo "sendkey up"; sleep 1; echo "sendkey ret"; sleep 3
  echo quit ) | run_qemu t3
echo "error screen:";            python3 tests/bios/vga.py "$W/t3a.vga"
echo "after another key (x):";   python3 tests/bios/vga.py "$W/t3b.vga"
echo "after Enter (menu back):"; python3 tests/bios/vga.py "$W/t3c.vga"
grep -a -E "boot failed|VBR" "$W/t3.serial" | tr -d '\r'
