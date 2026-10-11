#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# Boots an EFI file from a SECOND disk selected by PARTUUID (x86_64, QEMU).
# Usage: tests/uefi/run-uki.sh gpt|mbr|missing
set -eu
cd "$(dirname "$0")/../.."
CASE="${1:?usage: run-uki.sh gpt|mbr|missing}"
W="build/uefi-uki-test-$CASE"
rm -rf "$W"; mkdir -p "$W"

KIND=gpt; [ "$CASE" = mbr ] && KIND=mbr
UUID=$(python3 tests/uefi/mkdisk.py "$KIND" "$W/disk2.img")
[ "$CASE" = missing ] && UUID="deadbeef-0000-4000-8000-000000000000"

OFF=$((2048 * 512))
mkfs.fat -F 32 -n BBTEST --offset 2048 "$W/disk2.img" 131072 >/dev/null
mmd   -i "$W/disk2.img@@$OFF" ::/EFI ::/EFI/Linux
B="${BUILD:-release}"
make ARCH=x86_64 BUILD=$B >/dev/null      # builds blobs/x86_64/test.efi
mcopy -i "$W/disk2.img@@$OFF" blobs/x86_64/test.efi ::/EFI/Linux/linux-6.8.0-test.efi

cat > "$W/config.c" <<CFG
#include "binboot.h"
const struct bb_entry bb_entries[] = {
    { .name = "UKI on disk 2",
      .uki_path = "/EFI/Linux/linux-6.8.0-test.efi",
      .volume = "$UUID" },
};
const size_t bb_entry_count = sizeof(bb_entries) / sizeof(bb_entries[0]);
CFG
make ARCH=x86_64 BUILD=$B CONFIG_C="$W/config.c" >/dev/null
echo "PARTUUID: $UUID"

# Run QEMU in the background and wait for the outcome in the log instead of
# using fixed delays (boot time varies); then stop it.
QEMU_EXTRA="-drive file=$W/disk2.img,format=raw,if=virtio" \
    tools/run-qemu.sh x86_64 build/x86_64/$B-custom/BOOTX64.EFI \
    </dev/null > "$W/serial.log" 2>&1 &
QPID=$!
i=0
while [ $i -lt 240 ]; do
    grep -q -a -E "DEVHANDLE=|Press Enter" "$W/serial.log" 2>/dev/null && break
    sleep 0.5; i=$((i + 1))
done
sleep 1; kill $QPID 2>/dev/null || true; wait $QPID 2>/dev/null || true

python3 - "$W/serial.log" <<'PY'
import re, sys
t = open(sys.argv[1], "rb").read().decode("latin-1")
t = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "|", t)
for pat in (r"BinBoot: volume [^\r\n]*", r"Hello from embedded image", r"DEVHANDLE=\d",
            r"Boot failed: [^|\r\n]*", r"Cannot find or open the volume",
            r"Error code: 0x[0-9A-F]+"):
    for m in dict.fromkeys(re.findall(pat, t)):
        print(" ", m)
PY
