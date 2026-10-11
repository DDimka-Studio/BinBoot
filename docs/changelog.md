# Changelog

Versions look like `YYYY.MM.DD.release.stability.build`, for example
`2026.10.11.1.3.1`: year, month, day, release number, stability level (1 to 3, where 3 is
stable) and build number. The current version is set by `VERSION` at the
top of the `Makefile`.

## 2026.10.11.1.3.1

First public release.

### What it is

One boot loader file, configured at build time (`config/config.h` for
settings, `config/config.c` for boot entries). No files to read at boot.

### Added

- **UEFI** for x86_64, IA32 (32-bit firmware) and AArch64: text menu with a
  countdown (Up/Down/Enter, any key cancels), colors, and an error screen that
  waits for Enter.
- Boot entries: an EFI program or UKI **from a file** on any disk (chosen by
  PARTUUID, GPT or MBR), a Linux kernel with an EFI stub plus initrd and
  command line from a disk, and an EFI program **embedded in the loader**.
- Information screen before the menu (memory, CPU and its extensions,
  virtualization state, video adapter, display), with Pause/Break to hold it.
- **Legacy BIOS** (x86, MBR disks): menu, chainloading a partition's boot
  sector, Linux from raw sectors (`tools/blocklist.py`), and an installer
  (`tools/install-bios.sh`) that backs up the old boot sector.
- `make build_uefi arch=native|x86_64|ia32|aarch64`, `make build_bios
  [part=N] [disk=/dev/sdX]` (`part=` makes partition N the first, default
  entry; `disk=` also installs), `make dist`.
- Documentation in `docs/` and a QEMU test suite in `tests/`.

### Known limits

- Tested in QEMU only, apart from the author's own machine (UEFI menu,
  information screen, a UKI from a file). Not tested: Secure Boot, Raspberry
  Pi 4, the Intel VT-x lines of the information screen on real hardware,
  Pause/Break reporting, the `sudo` step of `make build_bios disk=...`.
- IA32 cannot start 64-bit Linux kernels yet (only 32-bit EFI programs).
- BIOS: no GPT, no filesystem driver (Linux needs sector lists), no
  information screen, boot entries are MBR primary partitions 1-4.
- Not implemented: command shell, booting disk images, Multiboot, Device Tree
  for ARM boards, hooks for patches.
