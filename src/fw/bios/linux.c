/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * Linux x86 boot protocol (Documentation/arch/x86/boot.rst), real-mode
 * entry, protocol 2.06 or newer. Memory layout used:
 *
 *   0x20000  sector transfer buffer (BIOS reads here, then we copy up)
 *   0x90000  real-mode kernel setup code (boot sector + setup sectors)
 *   0x99000  kernel command line; the setup stack grows down from here
 *   0x100000 protected-mode kernel
 *   top of RAM  initrd
 */
#include "bios.h"
#include "disk.h"
#include "error.h"
#include "linux.h"
#include "log.h"

#define XFER_SEG        0x2000u
#define XFER_SECTORS    64u             /* 32 KiB, never crosses 64 KiB */
#define SETUP_BASE      0x90000u
#define CMDLINE_ADDR    0x99000u
#define HEAP_END        0x8E00u         /* heap_end_ptr; stack top is +0x200 */
#define KERNEL_ADDR     0x100000u
#define SECTOR          512u
#define EOF_STATUS      0x1000u

/* Offsets in the setup header (relative to the start of the kernel file). */
#define H_SETUP_SECTS   0x1F1u
#define H_VID_MODE      0x1FAu
#define H_BOOT_FLAG     0x1FEu
#define H_MAGIC         0x202u
#define H_VERSION       0x206u
#define H_TYPE_OF_LOADER 0x210u
#define H_LOADFLAGS     0x211u
#define H_RAMDISK_IMAGE 0x218u
#define H_RAMDISK_SIZE  0x21Cu
#define H_HEAP_END_PTR  0x224u
#define H_CMD_LINE_PTR  0x228u
#define H_INITRD_MAX    0x22Cu
#define H_CMDLINE_SIZE  0x238u
#define H_INIT_SIZE     0x260u

extern int bb_a20_enabled(void);
extern void bb_copy_phys(unsigned dst, unsigned src, unsigned bytes);

static unsigned get16(const unsigned char *p, unsigned o)
{
    return (unsigned)p[o] | ((unsigned)p[o + 1] << 8);
}

static unsigned get32(const unsigned char *p, unsigned o)
{
    return get16(p, o) | (get16(p, o + 2) << 16);
}

static void put16(unsigned char *p, unsigned o, unsigned v)
{
    p[o] = (unsigned char)v;
    p[o + 1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, unsigned o, unsigned v)
{
    put16(p, o, v & 0xFFFFu);
    put16(p, o + 2, v >> 16);
}

/* ---- sequential reading of an extent list ---- */

struct stream {
    const struct bb_extent *ext;
    unsigned count, idx, used;
    unsigned char drive;
};

static void stream_init(struct stream *s, const struct bb_extent *ext,
                        unsigned count, unsigned char drive)
{
    s->ext = ext;
    s->count = count;
    s->idx = 0;
    s->used = 0;
    s->drive = drive;
}

static unsigned total_sectors(const struct bb_extent *ext, unsigned count)
{
    unsigned i, n = 0;

    for (i = 0; i < count; i++)
        n += ext[i].sectors;
    return n;
}

/* Reads n sectors (n <= XFER_SECTORS) to seg:off. 0 = ok. */
static unsigned stream_read(struct stream *s, unsigned n, unsigned seg,
                            unsigned off)
{
    while (n > 0) {
        unsigned avail, take, err;

        while (s->idx < s->count && s->used >= s->ext[s->idx].sectors) {
            s->idx++;
            s->used = 0;
        }
        if (s->idx >= s->count)
            return EOF_STATUS;
        avail = s->ext[s->idx].sectors - s->used;
        take = n < avail ? n : avail;
        err = bb_disk_read(s->drive, s->ext[s->idx].lba + s->used, take,
                           seg, off);
        if (err != 0)
            return err;
        s->used += take;
        n -= take;
        off += take * SECTOR;
    }
    return 0;
}

/* Reads `sectors` sectors and stores them at the physical address dst. */
static unsigned stream_copy(struct stream *s, unsigned dst, unsigned sectors)
{
    while (sectors > 0) {
        unsigned n = sectors < XFER_SECTORS ? sectors : XFER_SECTORS;
        unsigned err = stream_read(s, n, XFER_SEG, 0);

        if (err != 0)
            return err;
        bb_copy_phys(dst, XFER_SEG << 4, n * SECTOR);
        dst += n * SECTOR;
        sectors -= n;
    }
    return 0;
}

static void report_read_error(const char *what, unsigned err)
{
    if (err == EOF_STATUS)
        bb_error_set(what, "The sector list is shorter than the file",
                     BB_ERR_NOCODE);
    else
        bb_error_set(what, "BIOS disk read failed (INT 13h status)", err);
}

/* ---- memory map ---- */

struct e820 {
    unsigned base_lo, base_hi, len_lo, len_hi, type, ext;
};

static int e820_read(unsigned *cont, struct e820 *e)
{
    unsigned eax = 0xE820, ebx = *cont, ecx = 24, edx = 0x534D4150, fail;

    e->ext = 1;
    __asm__ volatile(
        "pushl %%ebp\n\t"
        "int $0x15\n\t"
        "sbbl %%esi, %%esi\n\t"        /* esi = -1 if the call failed */
        "popl %%ebp"
        : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "=S"(fail)
        : "D"((unsigned)e)
        : "cc", "memory");
    if (fail != 0 || eax != 0x534D4150u)
        return 0;
    *cont = ebx;
    return 1;
}

/* End of the usable RAM region that contains 1 MiB, clamped below 4 GiB.
 * Returns 0 if the memory map could not be read. */
static unsigned ram_top(void)
{
    static struct e820 e;
    unsigned cont = 0, top = 0;

    do {
        unsigned end;

        if (!e820_read(&cont, &e))
            break;
        if (e.type != 1 || e.base_hi != 0 || e.base_lo > KERNEL_ADDR)
            continue;
        end = e.base_lo + e.len_lo;
        if (e.len_hi != 0 || end < e.base_lo)
            end = 0xFFFFF000u;          /* extends past 4 GiB */
        if (end > KERNEL_ADDR && end > top)
            top = end;
    } while (cont != 0);
    return top;
}

/* ---- A20 ---- */

static int a20_enable(void)
{
    unsigned ax, spin;
    unsigned char v;

    if (bb_a20_enabled())
        return 1;

    ax = 0x2401;                        /* BIOS: enable A20 gate */
    __asm__ volatile("pushl %%ebp\n\tint $0x15\n\tpopl %%ebp"
                     : "+a"(ax) : : "ebx", "ecx", "edx", "esi", "edi",
                       "cc", "memory");
    if (bb_a20_enabled())
        return 1;

    /* Fast A20 gate (port 0x92); bit 0 would reset the machine. */
    __asm__ volatile("inb $0x92, %0" : "=a"(v));
    __asm__ volatile("outb %0, $0x92" : : "a"((unsigned char)((v | 2u) & ~1u)));
    for (spin = 0; spin < 100000u; spin++)
        if (bb_a20_enabled())
            return 1;
    return 0;
}

/* ---- boot ---- */

__attribute__((noreturn))
static void run_kernel(void)
{
    /* Entry state required by the protocol: segments and stack in the
     * setup segment, CS:IP at setup code (setup segment + 0x20, offset 0). */
    __asm__ volatile(
        "cli\n\t"
        "movw $0x9000, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "movw $0x9000, %%sp\n\t"
        "ljmp $0x9020, $0x0000"
        : : : "memory");
    __builtin_unreachable();
}

void bb_bios_linux_boot(const struct bb_bios_linux *cfg, const char *cmdline)
{
    static unsigned char hdr[2 * SECTOR];
    char cl[256];
    struct stream ks, is;
    unsigned char drive = (cfg->drive == BB_BIOS_BOOT_DRIVE) ? bb_boot_drive
                                                             : cfg->drive;
    unsigned total_k, setup_total, pm_sectors, pm_bytes, version;
    unsigned init_size, top, limit, end, initrd_bytes = 0, initrd_sectors = 0;
    unsigned initrd_addr = 0, need, cl_max, i, err;

    /* --- read and check the setup header --- */
    total_k = total_sectors(cfg->kernel, cfg->kernel_count);
    if (cfg->kernel_count == 0 || total_k < 16) {
        bb_error_set("No kernel sectors are configured", 0, BB_ERR_NOCODE);
        return;
    }
    stream_init(&ks, cfg->kernel, cfg->kernel_count, drive);
    err = stream_read(&ks, 2, 0, (unsigned)hdr);
    if (err != 0) {
        report_read_error("Cannot read the kernel", err);
        return;
    }
    if (get16(hdr, H_BOOT_FLAG) != 0xAA55u ||
        get32(hdr, H_MAGIC) != 0x53726448u /* "HdrS" */) {
        bb_error_set("Not a Linux kernel image (no setup header)", 0,
                     BB_ERR_NOCODE);
        return;
    }
    version = get16(hdr, H_VERSION);
    if (version < 0x0206u) {
        bb_error_set("Kernel boot protocol is too old (need 2.06)", 0,
                     version);
        return;
    }
    if (!(hdr[H_LOADFLAGS] & 1u)) {
        bb_error_set("Only bzImage kernels are supported (not zImage)", 0,
                     BB_ERR_NOCODE);
        return;
    }
    setup_total = (hdr[H_SETUP_SECTS] != 0 ? hdr[H_SETUP_SECTS] : 4u) + 1u;
    if (setup_total < 2 || setup_total >= total_k) {
        bb_error_set("The kernel setup header is inconsistent", 0,
                     setup_total);
        return;
    }
    pm_sectors = total_k - setup_total;
    pm_bytes = pm_sectors * SECTOR;
    init_size = (version >= 0x020Au) ? get32(hdr, H_INIT_SIZE) : 0;
    need = init_size > pm_bytes ? init_size : pm_bytes;

    /* --- where does the initrd go? --- */
    if (cfg->initrd_count != 0) {
        initrd_sectors = total_sectors(cfg->initrd, cfg->initrd_count);
        initrd_bytes = cfg->initrd_size != 0 ? cfg->initrd_size
                                             : initrd_sectors * SECTOR;
        if (initrd_sectors == 0 || initrd_bytes > initrd_sectors * SECTOR) {
            bb_error_set("The initrd sector list does not match its size", 0,
                         BB_ERR_NOCODE);
            return;
        }
    }
    top = ram_top();
    if (top == 0) {
        bb_error_set("Cannot read the BIOS memory map (INT 15h E820)", 0,
                     BB_ERR_NOCODE);
        return;
    }
    if (initrd_bytes != 0) {
        limit = get32(hdr, H_INITRD_MAX);
        end = (limit == 0xFFFFFFFFu) ? limit : limit + 1u;
        if (end > top)
            end = top;
        if (initrd_bytes > end ||
            ((end - initrd_bytes) & 0xFFFFF000u) < KERNEL_ADDR + need) {
            bb_error_set("Not enough memory for the kernel and initrd", 0,
                         top);
            return;
        }
        initrd_addr = (end - initrd_bytes) & 0xFFFFF000u;
    } else if (KERNEL_ADDR + need > top) {
        bb_error_set("Not enough memory for the kernel", 0, top);
        return;
    }

    if (!a20_enable()) {
        bb_error_set("Cannot enable the A20 address line", 0, BB_ERR_NOCODE);
        return;
    }

    /* --- command line --- */
    cl_max = get32(hdr, H_CMDLINE_SIZE);
    if (cl_max == 0 || cl_max > sizeof(cl) - 1)
        cl_max = sizeof(cl) - 1;
    for (i = 0; i < sizeof(cl); i++)
        cl[i] = '\0';
    for (i = 0; cmdline != 0 && cmdline[i] != '\0' && i < cl_max; i++)
        cl[i] = cmdline[i];

    /* --- patch the header and place everything --- */
    hdr[H_TYPE_OF_LOADER] = 0xFF;                   /* "unknown loader" */
    hdr[H_LOADFLAGS] |= 0x80u;                      /* CAN_USE_HEAP */
    put16(hdr, H_HEAP_END_PTR, HEAP_END);
    put32(hdr, H_CMD_LINE_PTR, CMDLINE_ADDR);
    put16(hdr, H_VID_MODE, 0xFFFFu);                /* "normal" video */
    put32(hdr, H_RAMDISK_IMAGE, initrd_addr);
    put32(hdr, H_RAMDISK_SIZE, initrd_bytes);

    bb_copy_phys(SETUP_BASE, (unsigned)hdr, sizeof(hdr));
    bb_copy_phys(CMDLINE_ADDR, (unsigned)cl, sizeof(cl));

    err = stream_copy(&ks, SETUP_BASE + sizeof(hdr), setup_total - 2);
    if (err == 0)
        err = stream_copy(&ks, KERNEL_ADDR, pm_sectors);
    if (err != 0) {
        report_read_error("Cannot read the kernel", err);
        return;
    }
    if (initrd_bytes != 0) {
        stream_init(&is, cfg->initrd, cfg->initrd_count, drive);
        err = stream_copy(&is, initrd_addr, initrd_sectors);
        if (err != 0) {
            report_read_error("Cannot read the initrd", err);
            return;
        }
    }

    BB_LOG("BinBoot: starting the Linux kernel");
    bios_video_restore();
    run_kernel();
}
