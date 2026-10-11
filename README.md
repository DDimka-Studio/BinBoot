# BinBoot

A boot loader that is **one file** and is **configured when you build it**:
no config directory, no text file on the disk, nothing to edit at boot time.
The menu, the timeout, the texts and the colors are compiled in.

Runs on UEFI (x86_64, IA32, AArch64) and Legacy BIOS (x86, MBR disks). It can
start an EFI program or a UKI from any disk (picked by PARTUUID), a Linux kernel
with an initrd, a program stored inside the loader itself, and on BIOS it can
chainload a boot sector or start Linux from raw sectors.

## Quick start

```
make build_uefi arch=native          # needs: make, clang, lld
$EDITOR config/config.c              # what the menu offers
$EDITOR config/config.h              # timeouts, texts, colors, switches
make build_uefi arch=native          # run again after every edit
```

Result: `build/x86_64/debug/BOOTX64.EFI` (or `BOOTIA32.EFI`, `BOOTAA64.EFI`).
Copy it to your EFI System Partition. For Legacy BIOS one command builds and installs:
`make build_bios build=release part=1 disk=/dev/sda`. Run make as a normal
user, not with `sudo`: it asks for root itself only for the final write. Write `arch=native`, not `-arch=native` (GNU make would
take the second form as its own option).

`make help` lists the targets.

## Documentation

See [docs/](docs/README.md): building, configuration (every option), installing,
troubleshooting (error codes), testing, internals.

## Status

Tested in QEMU only, apart from the author's own real machine for the UEFI menu,
information screen and booting a UKI from a file. Secure Boot, Raspberry Pi 4
and VT-x detection on real Intel CPUs are untested. The docs list what is
missing.

## License

BinBoot is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License, version 3 only (SPDX:
`GPL-3.0-only`). It is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; see the [LICENSE](LICENSE) file for details.

Copyright (C) 2026 DDimka-Studio - <https://github.com/DDimka-Studio>

Programs you start with BinBoot, or store inside it with `BB_EMBED` (a UKI,
Windows, another loader), keep their own licenses.

Releases are source archives on purpose: the configuration is compiled in, so
every user builds their own single file (see Quick start).

