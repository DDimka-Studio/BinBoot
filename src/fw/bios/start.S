/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* BinBoot stage 2 entry: real mode, loaded at 0000:8000, DL = boot drive. */
    .code16
    .section .text.start, "ax"
    .globl _start
_start:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movl    $0x7C00, %esp           /* Stack grows down from the MBR slot. */
    sti
    cld

    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorb    %al, %al
    rep stosb

    movb    %dl, bb_boot_drive      /* After bss is cleared. */
    calll   bb_bios_main
1:  hlt
    jmp     1b
