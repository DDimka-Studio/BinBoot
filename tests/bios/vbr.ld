/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
ENTRY(_start)
SECTIONS { . = 0x7C00; .text : { *(.text) } /DISCARD/ : { *(.comment) *(.note*) } }
