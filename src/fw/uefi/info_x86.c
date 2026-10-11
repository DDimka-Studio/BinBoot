/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* CPU lines for x86 (IA32 and x86_64): CPUID, plus MSRs for virtualization. */
#include <cpuid.h>

#include "config.h"
#include "info.h"           /* src/core/info.h */
#include "uefi_info.h"

#define MSR_IA32_FEATURE_CONTROL 0x3Au
#define MSR_VMX_PROCBASED_CTLS   0x482u
#define MSR_VMX_PROCBASED_CTLS2  0x48Bu
#define MSR_VM_CR                0xC0010114u

/* Unused when the virtualization line is switched off in the config. */
__attribute__((unused))
static void rdmsr(unsigned msr, unsigned *lo, unsigned *hi)
{
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

#if BB_INFO_CPU
static void info_cpu_name(void)
{
    unsigned r[12], i, a, b, c, d;
    char name[49];
    const char *p;
    struct bb_sb v;

    bb_sb_init(&v);
    __cpuid(0x80000000u, a, b, c, d);
    if (a >= 0x80000004u) {
        for (i = 0; i < 3; i++)
            __cpuid(0x80000002u + i, r[i * 4], r[i * 4 + 1], r[i * 4 + 2],
                    r[i * 4 + 3]);
        for (i = 0; i < 48; i++)
            name[i] = (char)(r[i / 4] >> (8u * (i % 4u)));
        name[48] = '\0';
        for (p = name; *p == ' '; p++)
            ;
        bb_sb_str(&v, p);
    } else {
        __cpuid(0, a, b, c, d);
        for (i = 0; i < 12; i++) {                 /* vendor: EBX EDX ECX */
            unsigned w = i < 4 ? b : i < 8 ? d : c;
            name[i] = (char)(w >> (8u * (i % 4u)));
        }
        name[12] = '\0';
        bb_sb_str(&v, name);
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
    unsigned max, a, b, c1 = 0, d1 = 0, b7 = 0, c, d, ext_max, c8 = 0, d8 = 0;
    struct bb_sb v;

    bb_sb_init(&v);
    max = __get_cpuid_max(0, 0);
    if (max >= 1)
        __cpuid(1, a, b, c1, d1);
    if (max >= 7)
        __cpuid_count(7, 0, a, b7, c, d);
    ext_max = __get_cpuid_max(0x80000000u, 0);
    if (ext_max >= 0x80000001u)
        __cpuid(0x80000001u, a, b, c8, d8);

    add_flag(&v, (d1 >> 26) & 1, "SSE2");
    add_flag(&v, (c1 >> 19) & 1, "SSE4.1");
    add_flag(&v, (c1 >> 20) & 1, "SSE4.2");
    add_flag(&v, (c1 >> 25) & 1, "AES");
    add_flag(&v, (c1 >> 28) & 1, "AVX");
    add_flag(&v, (b7 >> 5) & 1, "AVX2");
    add_flag(&v, (b7 >> 16) & 1, "AVX512F");
    add_flag(&v, (b7 >> 8) & 1, "BMI2");
    add_flag(&v, (d8 >> 20) & 1, "NX");
    add_flag(&v, (d8 >> 29) & 1, "x86-64");
    bb_info_add(BB_INFO_OK, BB_INFO_LABEL_FEATURES, &v);
}
#endif

#if BB_INFO_VIRT
static void info_virt(void)
{
    unsigned max, a, b, c1 = 0, d1, ext_max, c8 = 0, d8, lo, hi;
    struct bb_sb v;

    bb_sb_init(&v);
    max = __get_cpuid_max(0, 0);
    if (max >= 1)
        __cpuid(1, a, b, c1, d1);
    ext_max = __get_cpuid_max(0x80000000u, 0);
    if (ext_max >= 0x80000001u)
        __cpuid(0x80000001u, a, b, c8, d8);

    if ((c1 >> 5) & 1u) {                          /* Intel VMX */
        rdmsr(MSR_IA32_FEATURE_CONTROL, &lo, &hi);
        if ((lo & 1u) && !(lo & 4u)) {             /* locked, VMX off */
            bb_sb_str(&v, "Intel VT-x is disabled in the firmware setup");
            bb_info_add(BB_INFO_FAIL, BB_INFO_LABEL_VIRT, &v);
            return;
        }
        bb_sb_str(&v, "Intel VT-x");
        rdmsr(MSR_VMX_PROCBASED_CTLS, &lo, &hi);
        if (hi & 0x80000000u) {                    /* secondary controls */
            rdmsr(MSR_VMX_PROCBASED_CTLS2, &lo, &hi);
            if (hi & 2u)
                bb_sb_str(&v, ", EPT");
            if (hi & 0x80u)
                bb_sb_str(&v, ", unrestricted guest");
        }
        bb_info_add(BB_INFO_OK, BB_INFO_LABEL_VIRT, &v);
    } else if ((c8 >> 2) & 1u) {                   /* AMD SVM */
        rdmsr(MSR_VM_CR, &lo, &hi);
        if (lo & 0x10u) {                          /* SVMDIS */
            bb_sb_str(&v, "AMD-V is disabled in the firmware setup");
            bb_info_add(BB_INFO_FAIL, BB_INFO_LABEL_VIRT, &v);
            return;
        }
        bb_sb_str(&v, "AMD-V");
        if (__get_cpuid_max(0x80000000u, 0) >= 0x8000000Au) {
            __cpuid(0x8000000Au, a, b, c1, d8);
            if (d8 & 1u)
                bb_sb_str(&v, ", nested paging");
        }
        bb_info_add(BB_INFO_OK, BB_INFO_LABEL_VIRT, &v);
    } else {
        bb_sb_str(&v, "not supported by this CPU");
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
