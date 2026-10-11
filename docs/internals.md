# Internals

For people who want to read or patch the source.

## Layout

```
config/            config.h (settings), config.c (boot entries)
include/binboot.h  types shared by the loader and the config (bb_entry, ...)
src/core/          firmware-independent code
  menu.c           the menu, countdown, key handling
  info.c           information-screen lines (bb_info_add, bb_info_draw)
  error.c          the error screen (bb_error_set, bb_error_screen)
  log.c            debug logging (debug builds only)
src/fw/uefi/       UEFI: efi.h (hand-written definitions), main.c, console.c,
                   boot.c (EFI files, kernels, volumes), info*.c (hardware info)
src/fw/bios/       BIOS: mbr.S (stage 1), start.S + unreal.S (stage 2 entry and
                   memory helpers), main.c, console.c, disk.c, linux.c
mk/bios.mk         the BIOS build (called by the top Makefile)
tools/             install-bios.sh, blocklist.py, run-qemu.sh, a test payload
tests/             the test scripts (see testing.md)
blobs/<arch>/      programs embedded with BB_EMBED (created by you, see embedding.md)
```

No libc and no external UEFI library is used: the few protocol definitions that
are needed are written by hand in `efi.h`.

## How a UEFI boot goes

`efi_main` in `main.c`: set up the console, optionally show the information
screen (`bb_info_*`), then loop: `bb_menu_run` returns the chosen entry, the
matching function in `boot.c` runs it. On success of a program that exits
normally BinBoot returns to the firmware; on failure the error is recorded with
`bb_error_set`, `bb_error_screen` shows it, and the loop continues without a
countdown.

Entry types are tried in the order `.image`, `.uki_path`, `.kernel`. A kernel is
loaded with `LoadImage` from memory, its command line is passed as load options,
and the initrd is served through `LINUX_EFI_INITRD_MEDIA_GUID` /
`EFI_LOAD_FILE2_PROTOCOL`. An EFI file is loaded from memory with a device path
built from its volume, so the program gets a proper `DeviceHandle`.

## How a BIOS boot goes

The MBR code (440 bytes) loads stage 2 from the sectors after it to `0000:8000`
and jumps there. Stage 2 is 16-bit real-mode C (`clang -m16`): it draws the menu
straight into VGA memory, and boots either by copying a boot sector to `0x7C00`
(chainload) or by running the Linux boot protocol: setup code at `0x90000`,
kernel at 1 MiB, initrd at the top of RAM. Memory above 1 MiB is reached through
"unreal mode" (`unreal.S`).

## The console abstraction

`struct bb_console` (in `src/core/menu.h`) is everything the menu, the
information screen and the error screen need from the firmware: `clear`,
`put`, `clear_line`, `rows`, `poll_key`, `sleep_ms`, `set_attr`. A new platform
needs one implementation of it.

## Extension points

- `bb_info_add(status, label, &value)` adds a line to the information screen;
  call it before `bb_info_draw`.
- `BB_EMBED` links a file into the loader (see configuration.md).
- `bb_error_set` / `bb_error_screen` report a failure from new boot code.

## Adding a CPU or firmware

A UEFI architecture is a block in the top `Makefile` (clang `-target`, the
`lld-link` machine, the output name) plus, if it differs, a calling-convention
line in `efi.h` and CPU-information code in `src/fw/uefi/info_*.c`.
