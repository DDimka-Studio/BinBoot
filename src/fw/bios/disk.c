/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "disk.h"

struct dap {
    unsigned char size, zero;
    unsigned short count, offset, segment;
    unsigned lba_lo, lba_hi;
} __attribute__((packed));

unsigned bb_disk_read(unsigned char drive, unsigned lba, unsigned count,
                      unsigned seg, unsigned off)
{
    static struct dap dap;
    unsigned ax = 0, tries;
    unsigned char failed;

    for (tries = 0; tries < 3; tries++) {
        dap.size = 0x10;
        dap.zero = 0;
        dap.count = (unsigned short)count;
        dap.offset = (unsigned short)off;
        dap.segment = (unsigned short)seg;
        dap.lba_lo = lba;
        dap.lba_hi = 0;

        ax = 0x4200;
        __asm__ volatile(
            "pushl %%ebp\n\t"
            "int $0x13\n\t"
            "setc %%cl\n\t"
            "popl %%ebp"
            : "+a"(ax), "=c"(failed)
            : "d"((unsigned)drive), "S"((unsigned)&dap)
            : "ebx", "edi", "cc", "memory");
        if (!failed)
            return 0;
    }
    ax = (ax >> 8) & 0xFFu;
    return ax != 0 ? ax : 0xFFu;
}
