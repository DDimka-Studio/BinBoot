#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# 32-bit UEFI (OVMF IA32 in QEMU): embedded image and EFI file from disk.
# Usage: tests/uefi/run-ia32.sh embedded|file
set -eu
cd "$(dirname "$0")/../.."
CASE="${1:?usage: run-ia32.sh embedded|file}"
W="build/uefi-ia32-test-$CASE"
rm -rf "$W"; mkdir -p "$W/esp/EFI/Linux"

make ARCH=ia32 BUILD=debug >/dev/null            # also builds blobs/ia32/test.efi
cp blobs/ia32/test.efi "$W/esp/EFI/Linux/test.efi"

if [ "$CASE" = embedded ]; then
cat > "$W/config.c" <<CFG
#include "binboot.h"
BB_EMBED(blob_demo, "blobs/ia32/test.efi");
const struct bb_entry bb_entries[] = { { .name = "Embedded", .image = &blob_demo } };
const size_t bb_entry_count = sizeof(bb_entries) / sizeof(bb_entries[0]);
CFG
else
cat > "$W/config.c" <<CFG
#include "binboot.h"
const struct bb_entry bb_entries[] = {
    { .name = "File", .uki_path = "/EFI/Linux/test.efi" } };
const size_t bb_entry_count = sizeof(bb_entries) / sizeof(bb_entries[0]);
CFG
fi
make ARCH=ia32 BUILD=debug CONFIG_C="$W/config.c" >/dev/null

timeout 90 tools/run-qemu.sh ia32 build/ia32/debug-custom/BOOTIA32.EFI "$W/esp" \
    </dev/null > "$W/serial.log" 2>&1 || true
python3 - "$W/serial.log" <<'PY'
import re, sys
t = open(sys.argv[1], "rb").read().decode("latin-1")
t = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "|", t)
for pat in (r"BinBoot: [^\r\n|]*", r"Hello from embedded image", r"DEVHANDLE=\d",
            r"Boot failed[^|\r\n]*", r"Error code: 0x[0-9A-F]+"):
    for m in dict.fromkeys(re.findall(pat, t)):
        print(" ", m)
PY
