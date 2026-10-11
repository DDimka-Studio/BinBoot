/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* CPU lines for AArch64: identification and feature system registers. */
#include <stdint.h>

#include "config.h"
#include "info.h"           /* src/core/info.h */
#include "uefi_info.h"

__attribute__((unused))
static uint64_t read_midr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(v));
    return v;
}

__attribute__((unused))
static uint64_t read_isar0(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, id_aa64isar0_el1" : "=r"(v));
    return v;
}

__attribute__((unused))
static uint64_t read_current_el(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
    return v >> 2;
}

#if BB_INFO_CPU
static const char *implementer(unsigned id)
{
    switch (id) {
    case 0x41: return "ARM";
    case 0x42: return "Broadcom";
    case 0x43: return "Cavium";
    case 0x46: return "Fujitsu";
    case 0x50: return "APM";
    case 0x51: return "Qualcomm";
    case 0x61: return "Apple";
    case 0x69: return "Intel";
    default:   return 0;
    }
}

static const char *arm_part(unsigned part)
{
    switch (part) {
    case 0xd03: return "Cortex-A53";
    case 0xd05: return "Cortex-A55";
    case 0xd07: return "Cortex-A57";
    case 0xd08: return "Cortex-A72";
    case 0xd09: return "Cortex-A73";
    case 0xd0b: return "Cortex-A76";
    case 0xd0c: return "Neoverse N1";
    default:    return 0;
    }
}

static void info_cpu_name(void)
{
    uint64_t midr = read_midr();
    unsigned impl = (unsigned)((midr >> 24) & 0xFFu);
    unsigned part = (unsigned)((midr >> 4) & 0xFFFu);
    const char *name = impl == 0x41 ? arm_part(part) : 0;
    const char *vendor = implementer(impl);
    struct bb_sb v;

    bb_sb_init(&v);
    if (name != 0) {
        bb_sb_str(&v, name);
    } else {
        bb_sb_str(&v, vendor != 0 ? vendor : "implementer 0x");
        if (vendor == 0)
            bb_sb_hex(&v, impl, 2);
        bb_sb_str(&v, ", part 0x");
        bb_sb_hex(&v, part, 3);
    }
    bb_info_add(BB_INFO_OK, BB_INFO_LABEL_CPU, &v);
}
#endif

#if BB_INFO_FEATURES
static void add_flag(struct bb_sb *v, int present, const char *name)
{
    if (!present)
        return;
    if (v->len != 0)
        bb_sb_str(v, " ");
    bb_sb_str(v, name);
}

static void info_cpu_features(void)
{
    uint64_t isar0 = read_isar0();
    struct bb_sb v;

    bb_sb_init(&v);
    add_flag(&v, ((isar0 >> 4) & 0xF) != 0, "AES");
    add_flag(&v, ((isar0 >> 12) & 0xF) != 0, "SHA2");
    add_flag(&v, ((isar0 >> 16) & 0xF) != 0, "CRC32");
    add_flag(&v, ((isar0 >> 20) & 0xF) != 0, "LSE");
    bb_info_add(BB_INFO_OK, BB_INFO_LABEL_FEATURES, &v);
}
#endif

#if BB_INFO_VIRT
static void info_virt(void)
{
    struct bb_sb v;
    uint64_t el = read_current_el();

    bb_sb_init(&v);
    if (el >= 2) {
        bb_sb_str(&v, "running at EL2 (hypervisor level available)");
        bb_info_add(BB_INFO_OK, BB_INFO_LABEL_VIRT, &v);
    } else {
        bb_sb_str(&v, "running at EL1 (no hypervisor level)");
        bb_info_add(BB_INFO_WARN, BB_INFO_LABEL_VIRT, &v);
    }
}
#endif

void uefi_info_cpu(void)
{
#if BB_INFO_CPU
    info_cpu_name();
#endif
#if BB_INFO_FEATURES
    info_cpu_features();
#endif
#if BB_INFO_VIRT
    info_virt();
#endif
}
