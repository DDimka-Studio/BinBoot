#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# Installs BinBoot (Legacy BIOS) onto a disk or a disk image.
#
#   tools/install-bios.sh [--yes] [--force] [--build DIR] [--backup FILE] [--part N] TARGET
#
# TARGET is a whole disk (e.g. /dev/sdb, NOT a partition) or an image file.
# Only the MBR boot code (first 440 bytes) and the gap before the first
# partition are written. The disk signature and partition table stay
# untouched, and the old first sector is saved to a backup file first.
set -eu

die() { echo "error: $*" >&2; exit 1; }

BUILD_DIR=build/bios/release
YES=0
FORCE=0
BACKUP=""
PART=""

while [ $# -gt 1 ]; do
    case "$1" in
        --yes)    YES=1 ;;
        --force)  FORCE=1 ;;
        --build)  BUILD_DIR="$2"; shift ;;
        --backup) BACKUP="$2"; shift ;;
        --part)   PART="$2"; shift ;;
        *) die "unknown option: $1" ;;
    esac
    shift
done
[ $# -eq 1 ] || die "usage: $0 [--yes] [--force] [--build DIR] [--backup FILE] [--part N] TARGET"
TARGET="$1"

MBR_BIN="$BUILD_DIR/mbr.bin"
STAGE2="$BUILD_DIR/stage2.bin"
[ -f "$MBR_BIN" ] && [ -f "$STAGE2" ] || \
    die "missing $MBR_BIN or $STAGE2 (run 'make bios BUILD=release' first)"
[ -e "$TARGET" ] || die "$TARGET does not exist"
[ -w "$TARGET" ] || die "$TARGET is not writable (try sudo)"

# BIOS starts the first sector of a whole disk, so a partition is the wrong target.
if [ -b "$TARGET" ] && [ -e "/sys/class/block/$(basename "$TARGET")/partition" ]; then
    die "$TARGET is a partition; give the whole disk (e.g. /dev/sda, not /dev/sda1)"
fi

if [ -b "$TARGET" ] && grep -q "^$TARGET" /proc/mounts 2>/dev/null; then
    die "$TARGET (or one of its partitions) is mounted; unmount it first"
fi

S2_SECTORS=$(( ($(wc -c < "$STAGE2") + 511) / 512 ))
NEED=$(( S2_SECTORS + 1 ))        # sector 0 is the MBR

SECTOR0=$(mktemp)
trap 'rm -f "$SECTOR0"' EXIT
dd if="$TARGET" of="$SECTOR0" bs=512 count=1 2>/dev/null

u8()  { od -An -tu1 -j "$2" -N1 "$1" | tr -d ' \n'; }
u32() { od -An -tu4 --endian=little -j "$2" -N4 "$1" | tr -d ' \n'; }

SIG=$(od -An -tx1 -j 510 -N2 "$SECTOR0" | tr -d ' \n')
if [ "$SIG" != "55aa" ] && [ "$FORCE" -ne 1 ]; then
    die "no valid MBR partition table found (use --force for a blank disk)"
fi

FIRST=0
for i in 0 1 2 3; do
    off=$(( 446 + 16 * i ))
    type=$(u8 "$SECTOR0" $(( off + 4 )))
    start=$(u32 "$SECTOR0" $(( off + 8 )))
    [ "$type" -eq 238 ] && \
        die "GPT disk detected: BIOS boot needs a 'BIOS boot' partition (not supported yet)"
    if [ "$type" -ne 0 ] && [ "$start" -ne 0 ]; then
        if [ "$FIRST" -eq 0 ] || [ "$start" -lt "$FIRST" ]; then
            FIRST=$start
        fi
    fi
done
if [ "$FIRST" -ne 0 ] && [ "$FIRST" -lt "$NEED" ]; then
    die "only $(( FIRST - 1 )) free sectors before the first partition, need $S2_SECTORS"
fi

# --part N: the partition the menu will boot first must really exist.
if [ -n "$PART" ]; then
    poff=$(( 446 + 16 * (PART - 1) ))
    ptype=$(u8 "$SECTOR0" $(( poff + 4 )))
    pstart=$(u32 "$SECTOR0" $(( poff + 8 )))
    if [ "$ptype" -eq 0 ] || [ "$pstart" -eq 0 ]; then
        die "partition $PART of $TARGET is empty or does not exist"
    fi
fi

[ -n "$BACKUP" ] || BACKUP="bb-mbr-backup-$(basename "$TARGET").bin"
echo "Target:        $TARGET"
echo "Stage 2:       $S2_SECTORS sectors at LBA 1"
echo "MBR backup:    $BACKUP"
if [ "$YES" -ne 1 ]; then
    printf 'Install BinBoot? This overwrites the boot code of %s [y/N] ' "$TARGET"
    read -r answer
    [ "$answer" = "y" ] || [ "$answer" = "Y" ] || die "aborted"
fi

cp "$SECTOR0" "$BACKUP"
dd if="$STAGE2" of="$TARGET" bs=512 seek=1 conv=notrunc 2>/dev/null
dd if="$MBR_BIN" of="$TARGET" bs=440 count=1 conv=notrunc 2>/dev/null
sync
echo "Done."
