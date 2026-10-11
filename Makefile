# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 DDimka-Studio
# BinBoot build system. Needs clang, lld (lld-link / ld.lld) and make.
# Run `make help` for the short version of this.
#
#   make build_uefi [arch=native|x86_64|ia32|aarch64] [build=debug|release]
#   make build_bios [build=debug|release]
#
# Plain make words must not start with "-" (GNU make would take them as its
# own options), so variables are written arch=... and build=...
# The upper-case spellings (ARCH=, BUILD=) work as well.

# Included .d files (below) define rules before `all`, so name the default.
.DEFAULT_GOAL := all

# Release version: YYYY.MM.DD.release.stability.build (see docs/README.md).
VERSION := 2026.10.11.1.3.1

# ---- user-facing variables ----
ifneq ($(arch),)
ARCH := $(arch)
endif
ifneq ($(build),)
BUILD := $(build)
endif
ARCH  ?= x86_64
BUILD ?= debug

# arch=native: the architecture of the machine running make.
ifeq ($(ARCH),native)
HOST_ARCH := $(shell uname -m)
ifeq ($(HOST_ARCH),x86_64)
override ARCH := x86_64
else ifneq ($(filter aarch64 arm64,$(HOST_ARCH)),)
override ARCH := aarch64
else ifneq ($(filter i386 i486 i586 i686,$(HOST_ARCH)),)
override ARCH := ia32
else
$(error Cannot map the host architecture '$(HOST_ARCH)'; use arch=x86_64, ia32 or aarch64)
endif
endif

ifeq ($(filter debug release,$(BUILD)),)
$(error build must be debug or release, not '$(BUILD)')
endif

# ---- tools ----
# Own variable names (not CC/LD) so a CC=gcc in your shell cannot get in the
# way. Override when a tool is not on PATH:  make CLANG=/opt/llvm/bin/clang
CLANG    ?= clang
LLD_LINK ?= lld-link
LD_LLD   ?= ld.lld

ifeq ($(filter help clean,$(MAKECMDGOALS)),)
define need_tool
$(if $(shell command -v $(1) 2>/dev/null),,$(error '$(1)' not found. If you ran `sudo make ...`: sudo resets PATH, so run make as a normal user - the install step asks for root by itself. Otherwise install clang and lld, or point to the tool: make $(2)=/path/to/$(1)))
endef
$(call need_tool,$(CLANG),CLANG)
ifeq ($(filter build_bios bios,$(MAKECMDGOALS)),)
$(call need_tool,$(LLD_LINK),LLD_LINK)
else
$(call need_tool,$(LD_LLD),LD_LLD)
endif
ifneq ($(SUDO_USER),)
$(warning Building under sudo leaves root-owned files in build/. It is not needed: run make as a normal user, only the BIOS install step asks for root.)
endif
endif

# CONFIG_C can point at another configuration (used by the tests). Such
# builds go to their own directory so they never mix with normal ones.
CONFIG_C ?= config/config.c
VARIANT  := $(if $(filter-out config/config.c,$(CONFIG_C)),-custom)

ifeq ($(ARCH),x86_64)
TRIPLE  := x86_64-unknown-windows
MACHINE := x64
EFINAME := BOOTX64.EFI
ARCHFLAGS := -mno-red-zone
else ifeq ($(ARCH),ia32)
TRIPLE  := i386-unknown-windows
MACHINE := x86
EFINAME := BOOTIA32.EFI
# 32-bit UEFI may not have SSE enabled, and C names get a "_" prefix.
ARCHFLAGS := -mno-sse -mno-mmx -DBB_SYM_PREFIX=\"_\"
else ifeq ($(ARCH),aarch64)
TRIPLE  := aarch64-unknown-windows
MACHINE := arm64
EFINAME := BOOTAA64.EFI
ARCHFLAGS :=
else
$(error Unsupported arch '$(ARCH)': use native, x86_64, ia32 or aarch64)
endif

CC     := $(CLANG)
LINK   := $(LLD_LINK)
OUT    := build/$(ARCH)/$(BUILD)$(VARIANT)
TARGET := $(OUT)/$(EFINAME)

# EXTRA_CFLAGS comes first, e.g. -Imy_config_dir to override config.h.
CFLAGS := $(EXTRA_CFLAGS) -target $(TRIPLE) -std=c11 -ffreestanding -fshort-wchar \
          $(ARCHFLAGS) -fno-stack-protector -fno-builtin \
          -MMD -MP -Wall -Wextra -Werror -DBB_FW_UEFI -DBB_ARCH=\"$(ARCH)\" \
          -Iinclude -Iconfig -Isrc/core -Isrc/fw/uefi
ifeq ($(BUILD),debug)
CFLAGS += -O1 -g -DBB_LOG_ENABLED
else
CFLAGS += -Os
endif

ENTRY ?= efi_main
LDFLAGS := -machine:$(MACHINE) -subsystem:efi_application -entry:$(ENTRY) \
           -nodefaultlib

ifeq ($(ARCH),aarch64)
INFO_CPU := src/fw/uefi/info_arm.c
else
INFO_CPU := src/fw/uefi/info_x86.c
endif
SRCS := src/fw/uefi/main.c src/fw/uefi/console.c src/fw/uefi/boot.c \
        src/fw/uefi/info.c $(INFO_CPU) src/core/menu.c src/core/error.c \
        src/core/info.c $(CONFIG_C)
ifeq ($(BUILD),debug)
SRCS += src/core/log.c
endif
OBJS  := $(patsubst %.c,$(OUT)/%.o,$(SRCS))
# Rebuild objects when a header they include (config.h, ...) changes.
-include $(OBJS:.o=.d)
BLOBS := blobs/$(ARCH)/test.efi

all: $(TARGET)

# Tiny EFI app used as the embedded demo payload.
blobs/$(ARCH)/test.efi: tools/testpayload/payload.c
	@mkdir -p blobs/$(ARCH) $(OUT)
	$(CC) $(CFLAGS) -c $< -o $(OUT)/payload.o
	$(LINK) $(LDFLAGS) -out:$@ $(OUT)/payload.o

# Rebuild the config object whenever an embedded file changes.
$(OUT)/$(CONFIG_C:.c=.o): $(BLOBS) $(wildcard blobs/$(ARCH)/*)

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(LINK) $(LDFLAGS) -out:$@ $(OBJS)

# ---- user-facing targets ----

# Prints where the result is, so it is easy to find.
build_uefi: $(TARGET)
	@echo "Built UEFI image: $(TARGET)"

# Legacy BIOS (x86 real mode): see mk/bios.mk. BIOS code exists for x86 only.
# disk=/dev/sdX also installs the result (asks first; yes=1 skips the question).
# Run as a normal user; only the write itself is run with sudo when needed.
# part=N: partition N becomes the first (default) menu entry. BIOS only.
ifneq ($(part),)
ifeq ($(filter 1 2 3 4,$(part)),)
$(error part must be 1, 2, 3 or 4 (an MBR primary partition), not '$(part)')
endif
ifeq ($(filter build_bios bios,$(MAKECMDGOALS)),)
$(error part= only applies to build_bios)
endif
endif
BIOS_OUT := build/bios/$(BUILD)$(VARIANT)$(if $(part),-p$(part))
build_bios: bios
	@echo "Built BIOS images: $(BIOS_OUT)/{mbr,stage2}.bin"
ifneq ($(disk),)
	@# Only the write needs root; the build above ran as the current user.
	@if [ -w "$(disk)" ] || [ "$$(id -u)" = 0 ]; then S=; else \
	    S=sudo; echo "Writing to $(disk) needs root: running the install step with sudo"; fi; \
	    $$S tools/install-bios.sh $(if $(yes),--yes) $(if $(part),--part $(part)) --build $(BIOS_OUT) $(disk)
else
	@echo "Install them with: make build_bios disk=/dev/sdX   (or tools/install-bios.sh)"
endif

bios:
ifeq ($(filter x86_64 ia32,$(ARCH)),)
	$(error The BIOS build is for x86 only (arch is '$(ARCH)'))
endif
	$(MAKE) -f mk/bios.mk BUILD=$(BUILD) CONFIG_C=$(CONFIG_C) PART=$(part) \
	    CLANG=$(CLANG) LD_LLD=$(LD_LLD)

help:
	@echo "BinBoot build targets:"
	@echo "  make build_uefi arch=native|x86_64|ia32|aarch64 [build=debug|release]"
	@echo "  make build_bios [build=debug|release] [part=1..4] [disk=/dev/sdX [yes=1]]"
	@echo "      part=N: partition N is the first (default) menu entry"
	@echo "      disk=:  also installs onto that whole disk (needs root)"
	@echo "  make run [arch=...]       boot the UEFI build in QEMU"
	@echo "  make dist                 source archive for a release"
	@echo "  make clean"
	@echo "Settings are in config/config.h and config/config.c (see docs/)."
	@echo "Note: write arch=native, not -arch=native (make treats - words as options)."

# make dist: binboot-<version>.tar.gz of the sources (for a release).
dist:
	@# Built in a temporary directory: tar must not write into what it reads.
	@t=$$(mktemp -d) && \
	tar czf $$t/binboot-$(VERSION).tar.gz \
	    --transform 's,^\.$$,binboot-$(VERSION),' \
	    --transform 's,^\./,binboot-$(VERSION)/,' \
	    --exclude=./build --exclude='./blobs/*/test.efi' --exclude='./binboot-*.tar.gz' \
	    --exclude='./bb-mbr-backup-*' --exclude=./.git . && \
	mv $$t/binboot-$(VERSION).tar.gz . && rmdir $$t
	@echo "Created binboot-$(VERSION).tar.gz"

bios-test:
	tests/bios/run.sh

# Boots a real EFI-stub kernel: make linux-test KERNEL=/path/to/vmlinuz
linux-test:
	tests/uefi/run-linux.sh $(KERNEL)

run: $(TARGET)
	tools/run-qemu.sh $(ARCH) $(TARGET)

clean:
	rm -rf build blobs/*/test.efi

.PHONY: all build_uefi build_bios bios help run clean bios-test linux-test dist
