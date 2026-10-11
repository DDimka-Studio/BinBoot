#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# Boots a BinBoot EFI binary in QEMU with UEFI firmware, serial on stdio.
# Usage: run-qemu.sh <x86_64|ia32|aarch64> <EFI file> [EXTRA_DIR]
# QEMU_EXTRA (environment, optional) adds raw qemu arguments, e.g. a 2nd disk.
# EXTRA_DIR (optional) is copied into the root of the virtual ESP.
set -eu
QEMU_EXTRA="${QEMU_EXTRA:-}"
ARCH="${1:?usage: run-qemu.sh <x86_64|ia32|aarch64> <EFI file>}"
EFI="${2:?usage: run-qemu.sh <x86_64|ia32|aarch64> <EFI file>}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/esp/EFI/BOOT"
if [ $# -ge 3 ]; then cp -r "$3"/. "$TMP/esp/"; fi

case "$ARCH" in
x86_64)
    cp "$EFI" "$TMP/esp/EFI/BOOT/BOOTX64.EFI"
    cp /usr/share/OVMF/OVMF_VARS_4M.fd "$TMP/vars.fd"
    exec qemu-system-x86_64 -machine q35 -m 512 -nographic \
        -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
        -drive if=pflash,format=raw,file="$TMP/vars.fd" \
        -drive format=raw,file=fat:rw:"$TMP/esp" $QEMU_EXTRA
    ;;
ia32)
    cp "$EFI" "$TMP/esp/EFI/BOOT/BOOTIA32.EFI"
    cp /usr/share/OVMF/OVMF32_VARS_4M.fd "$TMP/vars.fd"
    exec qemu-system-i386 -machine q35 -m 512 -nographic \
        -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF32_CODE_4M.fd \
        -drive if=pflash,format=raw,file="$TMP/vars.fd" \
        -drive format=raw,file=fat:rw:"$TMP/esp" $QEMU_EXTRA
    ;;
aarch64)
    cp "$EFI" "$TMP/esp/EFI/BOOT/BOOTAA64.EFI"
    # AArch64 pflash images must be exactly 64 MiB.
    cp /usr/share/AAVMF/AAVMF_CODE.fd "$TMP/code.fd"
    cp /usr/share/AAVMF/AAVMF_VARS.fd "$TMP/vars.fd"
    truncate -s 64M "$TMP/code.fd" "$TMP/vars.fd"
    exec qemu-system-aarch64 -machine virt -cpu cortex-a72 -m 512 -nographic \
        -drive if=pflash,format=raw,readonly=on,file="$TMP/code.fd" \
        -drive if=pflash,format=raw,file="$TMP/vars.fd" \
        -drive format=raw,file=fat:rw:"$TMP/esp" $QEMU_EXTRA
    ;;
*)
    echo "unsupported arch: $ARCH" >&2
    exit 1
    ;;
esac
