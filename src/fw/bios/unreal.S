/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * Helpers that reach memory above 1 MiB from real-mode code ("unreal
 * mode"): a flat 4 GiB limit is loaded into DS/ES once through protected
 * mode, and stays in effect after returning to real mode. All functions
 * follow the 32-bit cdecl convention used by clang -m16 (arguments pushed
 * as 32-bit values, return with retl).
 */
    .code16
    .text

/* Loads 4 GiB limits into DS and ES. Clobbers EAX, BX. Interrupts off. */
unreal_enter:
    cli
    lgdtl   bb_gdt_desc
    movl    %cr0, %eax
    orb     $1, %al
    movl    %eax, %cr0
    jmp     1f
1:  movw    $0x08, %bx
    movw    %bx, %ds
    movw    %bx, %es
    andb    $0xFE, %al
    movl    %eax, %cr0
    jmp     2f
2:  xorw    %bx, %bx
    movw    %bx, %ds                /* base 0 again, limit stays 4 GiB */
    movw    %bx, %es
    ret

/* void bb_copy_phys(unsigned dst, unsigned src, unsigned bytes);
 * Copies whole dwords between physical addresses below 4 GiB. */
    .globl  bb_copy_phys
bb_copy_phys:
    pushl   %ebp
    movl    %esp, %ebp
    pushfw
    pushl   %ebx
    pushl   %ecx
    pushl   %esi
    pushl   %edi
    call    unreal_enter
    movl    8(%ebp), %edi
    movl    12(%ebp), %esi
    movl    16(%ebp), %ecx
    shrl    $2, %ecx
    cld
    .byte   0x67                    /* address-size prefix: use ESI/EDI */
    rep movsl
    popl    %edi
    popl    %esi
    popl    %ecx
    popl    %ebx
    popfw
    popl    %ebp
    retl

/* int bb_a20_enabled(void): 1 if address line A20 is enabled. Compares a
 * byte at 0000:0500 with FFFF:0510 (= 1 MiB + 0x500 when A20 is on). */
    .globl  bb_a20_enabled
bb_a20_enabled:
    pushfw
    pushw   %ds
    pushw   %es
    pushl   %ebx
    pushl   %edx
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    decw    %ax
    movw    %ax, %es
    movb    0x0500, %bl
    movb    %es:0x0510, %bh
    movb    $0x00, 0x0500
    movb    $0xFF, %es:0x0510
    movb    0x0500, %dl             /* 0xFF here means the writes aliased */
    movb    %bl, 0x0500
    movb    %bh, %es:0x0510
    xorl    %eax, %eax
    cmpb    $0xFF, %dl
    setne   %al
    popl    %edx
    popl    %ebx
    popw    %es
    popw    %ds
    popfw
    retl

    .section .rodata
    .balign 8
bb_gdt:
    .quad   0                       /* null descriptor */
    .quad   0x00CF92000000FFFF      /* flat 4 GiB read/write data */
bb_gdt_desc:
    .word   bb_gdt_desc - bb_gdt - 1
    .long   bb_gdt
