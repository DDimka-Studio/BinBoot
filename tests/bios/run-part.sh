#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# `make build_bios part=N disk=...`: partition N becomes the first entry, the
# default one, and the entries that follow shift down. Checks the whole chain:
# build, install into an image, boot in QEMU, installer refusing an empty one.
set -eu
cd "$(dirname "$0")/../.."
W=build/bios-part-test
rm -rf "$W"; mkdir -p "$W"

for n in 1 2; do
    clang -target i386-pc-none-elf -DMSG="\"VBR:PART$n\"" -c tests/bios/vbr.S -o "$W/vbr$n.o"
    ld.lld -m elf_i386 -T tests/bios/vbr.ld --oformat binary -o "$W/vbr$n.bin" "$W/vbr$n.o"
done
fresh_image() { python3 tests/bios/mkimage.py "$1" "$W/vbr1.bin" "$W/vbr2.bin"; }

boot() {   # boot IMAGE LOGNAME SECONDS  (serial log only)
    ( sleep "$3"; echo quit ) | qemu-system-i386 -m 64 -display none -no-reboot \
        -serial "file:$W/$2.serial" -monitor stdio \
        -drive "file=$1,format=raw,if=ide" >/dev/null 2>&1 || true
    grep -a -o "VBR:PART[0-9]" "$W/$2.serial" | head -1
}

echo "--- no part=            -> first entry is partition 1"
fresh_image "$W/a.img"
make build_bios build=debug disk="$W/a.img" yes=1 >/dev/null
echo "booted: $(boot "$W/a.img" a 12)"

echo "--- part=2              -> partition 2 boots by default"
fresh_image "$W/b.img"
make build_bios build=debug part=2 disk="$W/b.img" yes=1 >/dev/null
echo "booted: $(boot "$W/b.img" b 12)"

echo "--- part=3 (empty)      -> installer must refuse"
fresh_image "$W/c.img"
if make build_bios build=debug part=3 disk="$W/c.img" yes=1 >"$W/c.out" 2>&1; then
    echo "NOT REFUSED"
else
    grep -a -o "partition 3 of .* is empty or does not exist" "$W/c.out" | head -1
fi
python3 - "$W/c.img" <<'PY'
import sys
d = open(sys.argv[1], "rb").read(512)
print("disk untouched after refusal:", d[:440] == bytes(440))
PY

echo "--- part=2 + one more entry -> the other entry shifts down"
sed 's|/\* Your entries go here. \*/|{ .name = "Second", .chain = { .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE, .partition = 1 } },|' \
    config/config.c > "$W/config.c"
fresh_image "$W/d.img"
make build_bios build=debug part=2 CONFIG_C="$W/config.c" disk="$W/d.img" yes=1 >/dev/null
( sleep 4; echo "pmemsave 0xb8000 4000 $W/d.vga"; sleep 1; echo quit ) | \
    qemu-system-i386 -m 64 -display none -no-reboot -monitor stdio \
        -drive "file=$W/d.img,format=raw,if=ide" >/dev/null 2>&1 || true
python3 tests/bios/vga.py "$W/d.vga"

rm -f bb-mbr-backup-*.img.bin
