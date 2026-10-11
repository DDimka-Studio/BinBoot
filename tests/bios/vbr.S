/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Test boot sector: reports MSG and the received DL on COM1 and screen. */
    .code16
    .text
    .globl _start
_start:
    movb    %dl, drv
    movw    $msg, %si
    call    puts
    movw    $dlmsg, %si
    call    puts
    movb    drv, %al
    shrb    $4, %al
    call    hex
    movb    drv, %al
    andb    $0x0F, %al
    call    hex
    movb    $'\r', %al
    call    putc
    movb    $'\n', %al
    call    putc
1:  hlt
    jmp     1b

puts:
    lodsb
    testb   %al, %al
    jz      2f
    call    putc
    jmp     puts
2:  ret

hex:
    cmpb    $10, %al
    jb      3f
    addb    $('A' - 10 - '0'), %al
3:  addb    $'0', %al
    /* fall through */
putc:
    pushw   %ax
    pushw   %dx
    movw    $0x3F8, %dx
    outb    %al, %dx
    popw    %dx
    popw    %ax
    movb    $0x0E, %ah
    xorw    %bx, %bx
    int     $0x10
    ret

msg:    .asciz MSG
dlmsg:  .asciz " DL="
drv:    .byte 0
    .org    510
    .word   0xAA55
