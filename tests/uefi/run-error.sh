#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# Boot failure on UEFI: error screen waits for Enter, then the menu returns.
set -eu
cd "$(dirname "$0")/.."/..
W=build/uefi-error-test; rm -rf "$W"; mkdir -p "$W"
make BUILD=release CONFIG_C=tests/uefi/config_err.c >/dev/null
# Wait until the error screen is really up (no fixed timing), then press a
# wrong key (must be ignored) and Enter.
( i=0; while [ $i -lt 120 ]; do
      grep -q "Press Enter" "$W/serial.log" 2>/dev/null && break
      sleep 0.5; i=$((i + 1))
  done
  sleep 1; printf 'x'; sleep 1; printf '\r'; sleep 5 ) | \
    timeout 90 tools/run-qemu.sh x86_64 build/x86_64/release-custom/BOOTX64.EFI \
    > "$W/serial.log" 2>&1 || true
python3 - "$W/serial.log" <<'PY'
import re, sys
t = open(sys.argv[1], "rb").read().decode("latin-1")
t = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "|", t)      # cursor moves -> separators
def pos(s, start=0): return t.find(s, start)
fail  = pos("Boot failed: Missing kernel")
code  = pos("Error code: 0x", fail)
prompt = pos("Press Enter to continue", fail)
menu2 = pos("Missing kernel", prompt + 5)
print("error title shown       :", fail >= 0)
print("file path shown         :", pos("/EFI/nothing/vmlinuz", fail) > 0)
print("error code shown        :", code > 0, re.findall(r"Error code: (0x[0-9A-F]+)", t))
print("prompt shown            :", prompt > 0)
print("menu back after Enter   :", menu2 > prompt)
print("no countdown after Enter:", pos("Booting in", prompt) < 0)
PY
