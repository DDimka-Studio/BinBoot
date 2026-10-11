#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# Information screen behaviour on x86_64 UEFI (QEMU): own screen that is
# replaced by the menu, key skip, hold key, colors. Waits for text in the
# log instead of fixed delays, so boot-time differences do not matter.
# Usage: tests/uefi/run-info.sh
set -eu
cd "$(dirname "$0")/../.."
W=build/uefi-info-test
rm -rf "$W"; mkdir -p "$W"

# build SECONDS HOLD_CHAR  -> $W/BOOT.EFI with a patched config.h
build() {
    rm -rf "$W/cfg" build/x86_64/release-custom; cp -r config "$W/cfg"
    sed -i "s/#define BB_INFO_SECONDS .*/#define BB_INFO_SECONDS $1/; s/#define BB_INFO_HOLD_CHAR .*/#define BB_INFO_HOLD_CHAR $2/" "$W/cfg/config.h"
    make ARCH=x86_64 BUILD=release CONFIG_C=tests/uefi/config_err.c \
        EXTRA_CFLAGS="-I$W/cfg" >/dev/null
    cp build/x86_64/release-custom/BOOTX64.EFI "$W/BOOT.EFI"
}

# shot NAME PATTERN SETTLE [DELAY KEY]...
#   start QEMU, wait until PATTERN shows in the log, send each KEY after its
#   DELAY seconds, wait SETTLE seconds, stop QEMU and print the screen.
shot() {
    name=$1; pat=$2; settle=$3; shift 3
    rm -f "$W/in"; mkfifo "$W/in"
    tools/run-qemu.sh x86_64 "$W/BOOT.EFI" < "$W/in" > "$W/$name.log" 2>&1 &
    qpid=$!
    exec 3> "$W/in"
    i=0
    while [ $i -lt 240 ]; do
        grep -q -a "$pat" "$W/$name.log" 2>/dev/null && break
        sleep 0.5; i=$((i + 1))
    done
    while [ $# -ge 2 ]; do sleep "$1"; printf "$2" >&3; shift 2; done
    sleep "$settle"
    kill "$qpid" 2>/dev/null || true
    exec 3>&-
    wait "$qpid" 2>/dev/null || true
    echo "--- $name"
    python3 tests/uefi/screen.py "$W/$name.log" | sed -n 1,9p
}

build 3 0
shot during_info "Display:" 0.3
shot after_info "Booting in" 0.5
build 30 "'p'"
shot hold_p "Display:" 3 1 p
shot hold_p_then_key "Display:" 2 1 p 1 x
build 30 0
shot skipped_by_key "Display:" 2 1 x

echo "--- colors (escape codes in front of the tags)"
python3 - "$W/during_info.log" <<'PY'
import re, sys
t = open(sys.argv[1], "rb").read().decode("latin-1")
seen = re.findall(r"((?:\x1b\[[0-9;]*m)+)(?:\x1b\[[0-9;]*H)?(\[  OK  \]|\[ WARN \]|\[ FAIL \])", t)
for esc, tag in dict.fromkeys(seen):
    print(" ", repr(esc), tag)
PY
