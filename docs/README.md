# BinBoot documentation

BinBoot is a boot loader that is **one file** and is **configured when it is
built**. There is no configuration directory, no text file on the disk and
nothing to edit at boot time: the menu, the timeout, the texts and the colors
are compiled into the loader. What you can do at boot is pick one of the
entries that were built in.

It runs on:

| Firmware | CPU | Output file |
|---|---|---|
| UEFI | x86_64 | `BOOTX64.EFI` |
| UEFI (32-bit firmware) | IA32 | `BOOTIA32.EFI` |
| UEFI | AArch64 | `BOOTAA64.EFI` |
| Legacy BIOS (MBR disks) | x86 | boot code in the MBR + a few sectors after it |

## Read next

| Document | What is in it |
|---|---|
| [building.md](building.md) | Requirements, `make build_uefi` / `make build_bios`, build variables |
| [configuration.md](configuration.md) | Every setting and every field of a boot entry, with examples |
| [installing.md](installing.md) | Putting BinBoot on a disk (UEFI, IA32, BIOS), undoing it |
| [troubleshooting.md](troubleshooting.md) | The error screen, error codes, common problems |
| [testing.md](testing.md) | The test scripts and exactly what they prove |
| [internals.md](internals.md) | Source layout and extension points for patches |
| [embedding.md](embedding.md) | Storing EFI programs inside the loader (`blobs/`, `BB_EMBED`) |
| [changelog.md](changelog.md) | What changed in each version |

## What works, and how well it is verified

Everything below was run in QEMU (OVMF, AAVMF, SeaBIOS). The author reports
that the UEFI menu, the information screen and booting a UKI from a file work
on a real machine; nothing else has been tried on real hardware.

| Feature | UEFI x86_64 | UEFI IA32 | UEFI AArch64 | BIOS |
|---|---|---|---|---|
| Menu, countdown, error screen | yes | yes | yes | yes |
| Information screen | yes | yes | yes | no |
| Boot an EFI file / UKI by path, on any disk (PARTUUID) | yes | yes (32-bit EFI files) | built, not run | - |
| Boot a Linux kernel + initrd from a disk | yes | no (64-bit kernels) | built, not run | raw sectors only |
| EFI program embedded in the loader | yes | yes | yes | - |
| Chainload another boot sector | - | - | - | yes |

Not implemented: a command shell, booting disk images, Multiboot, Device Tree
for ARM boards, FAT file access in the BIOS build, GPT disks in the BIOS
build, Secure Boot signing. Not tested: Secure Boot, Raspberry Pi 4, VT-x
detection on real Intel hardware.

## Versioning

`YYYY.MM.DD.release.stability.build`, e.g. `2026.10.11.1.3.1`: year, month, day,
release number, stability level and build number. Stability has three levels,
and 3 means stable (the maintainer's own judgement; what was actually verified
is listed above under "What works"). The current version is set by `VERSION` at the top of the
`Makefile`; `make dist` names the source archive after it.

## License

GPL-3.0-only, copyright (C) 2026 DDimka-Studio. See `LICENSE`. Programs that
BinBoot starts or that you embed with `BB_EMBED` keep their own licenses.

