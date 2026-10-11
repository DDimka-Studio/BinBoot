# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# BinBoot Legacy BIOS build. Invoked via `make bios [BUILD=debug|release]`.
BUILD ?= debug

# The included .d files define rules before `all`; without this line the
# first object would become the default goal and nothing else would be built.
.DEFAULT_GOAL := all
CONFIG_C ?= config/config.c
VARIANT  := $(if $(filter-out config/config.c,$(CONFIG_C)),-custom)
# PART (from part=N) becomes the first menu entry; it changes the compiled
# code, so it gets its own output directory.
PART  ?=
OUT   := build/bios/$(BUILD)$(VARIANT)$(if $(PART),-p$(PART))

# Tools come from the top Makefile (CLANG=, LD_LLD= to override).
CLANG  ?= clang
LD_LLD ?= ld.lld
CC := $(CLANG)
LD := $(LD_LLD)

CFLAGS := -target i386-pc-none-elf -m16 -std=c11 -ffreestanding -fno-pic \
          -fno-pie -fno-stack-protector -fno-builtin -fomit-frame-pointer \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -mno-sse -mno-mmx -MMD -MP -Wall -Wextra -Werror -DBB_FW_BIOS \
          $(if $(PART),-DBB_FIRST_PART=$(PART)) \
          -Iinclude -Iconfig -Isrc/core -Isrc/fw/bios
ifeq ($(BUILD),debug)
CFLAGS += -Os -DBB_LOG_ENABLED
else
CFLAGS += -Os
endif

SRCS := src/fw/bios/main.c src/fw/bios/console.c src/fw/bios/disk.c \
        src/fw/bios/linux.c src/core/menu.c src/core/error.c $(CONFIG_C)
ifeq ($(BUILD),debug)
SRCS += src/core/log.c
endif
-include $(patsubst %.c,$(OUT)/%.d,$(SRCS))
OBJS := $(OUT)/src/fw/bios/start.o $(OUT)/src/fw/bios/unreal.o \
        $(patsubst %.c,$(OUT)/%.o,$(SRCS))

# Stage 2 must fit between the MBR and the first partition (LBA 2048 is
# the usual start, leaving 63 sectors); 62 keeps one sector of margin.
MAX_STAGE2_SECTORS := 62

all: $(OUT)/mbr.bin $(OUT)/stage2.bin

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/src/fw/bios/%.o: src/fw/bios/%.S
	@mkdir -p $(dir $@)
	$(CC) -target i386-pc-none-elf -c $< -o $@

$(OUT)/stage2.bin: $(OBJS) src/fw/bios/stage2.ld
	$(LD) -m elf_i386 -T src/fw/bios/stage2.ld --oformat binary \
	      -o $@ $(OBJS)
	@n=$$(( ($$(stat -c %s $@) + 511) / 512 )); \
	if [ $$n -gt $(MAX_STAGE2_SECTORS) ]; then \
	    echo "stage2 is $$n sectors, limit is $(MAX_STAGE2_SECTORS)"; exit 1; \
	fi; \
	truncate -s $$(( n * 512 )) $@; echo "stage2: $$n sectors"

# The MBR needs the stage 2 sector count, so it is built after stage 2.
$(OUT)/mbr.bin: src/fw/bios/mbr.S src/fw/bios/mbr.ld $(OUT)/stage2.bin
	@n=$$(( $$(stat -c %s $(OUT)/stage2.bin) / 512 )); \
	$(CC) -target i386-pc-none-elf -DSTAGE2_SECTORS=$$n -c $< \
	      -o $(OUT)/mbr.o && \
	$(LD) -m elf_i386 -T src/fw/bios/mbr.ld --oformat binary \
	      -o $@ $(OUT)/mbr.o
	@test $$(stat -c %s $@) -eq 440 || { echo "mbr.bin must be 440 bytes"; exit 1; }

.PHONY: all
