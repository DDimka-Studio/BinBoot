/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * BinBoot stage 1: the 440 bytes of boot code that live in the MBR.
 * The disk signature (440..445) and the partition table (446..509) that
 * follow are NOT part of this file; the installer keeps the disk's own.
 *
 * Job: load stage 2 (sectors 1..STAGE2_SECTORS, right after the MBR) to
 * 0000:8000 with INT 13h extensions, then jump to it with DL = boot drive.
 * STAGE2_SECTORS is supplied by the build.
 */
    .code16
    .text
    .globl _start
_start:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x7C00, %sp
    sti
    cld
    movb    %dl, boot_drive

    /* Require INT 13h extensions (LBA). Every BIOS since ~1996 has them. */
    movb    $0x41, %ah
    movw    $0x55AA, %bx
    int     $0x13
    jc      fail_edd
    cmpw    $0xAA55, %bx
    jne     fail_edd

    movb    boot_drive, %dl
    movw    $dap, %si
    movb    $0x42, %ah
    int     $0x13
    jc      fail_read

    movb    boot_drive, %dl
    ljmp    $0x0000, $0x8000

fail_edd:
    movb    $'E', %al
    jmp     fail
fail_read:
    movb    $'R', %al
fail:
    movb    $0x0E, %ah
    xorw    %bx, %bx
    int     $0x10
hang:
    hlt
    jmp     hang

/* Disk Address Packet for INT 13h AH=42h. */
dap:
    .byte   0x10, 0
    .word   STAGE2_SECTORS
    .word   0x8000, 0x0000          /* offset, segment */
    .long   1, 0                    /* 64-bit starting LBA */
boot_drive:
    .byte   0

    .org    440                     /* Build fails if the code is larger. */
