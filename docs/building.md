# Building

## Requirements

- `make` (GNU make 4.x)
- `clang` and `lld` (the tools `lld-link` and `ld.lld` must be on `PATH`)
- Linux or another Unix-like host. Cross-compiling is built in: one host
  builds every target.

On Debian/Ubuntu: `sudo apt install make clang lld`.

**Run `make` as a normal user, not with `sudo`.** `sudo` resets `PATH`, so the
compiler that works in your shell is often not found under it, and everything
in `build/` would become root-owned. Only the BIOS install step needs root,
and `make` asks for it by itself (it runs just that one command with `sudo`).

If a tool is not on `PATH` (or only has a versioned name such as `clang-18`),
point to it:

```
make build_uefi arch=native CLANG=/usr/bin/clang-18 LLD_LINK=/usr/bin/lld-link-18
make build_bios LD_LLD=/usr/bin/ld.lld-18
```

The variables are `CLANG`, `LLD_LINK` (UEFI) and `LD_LLD` (BIOS). A `CC=` in
your environment is ignored on purpose.

The compiler is clang because a single `clang -target ...` produces the PE/COFF
files UEFI needs for x86_64, IA32 and AArch64, and the 16-bit code for BIOS.
Porting to GCC is possible in principle but has not been done.

## Commands

```
make build_uefi arch=native
make build_uefi arch=ia32 build=release
make build_bios
make build_bios build=release part=1 disk=/dev/sda        # build, then install (asks for root)
make help
make clean
```

| Variable | Values | Default | Meaning |
|---|---|---|---|
| `arch` | `native`, `x86_64`, `ia32`, `aarch64` | `x86_64` | Target CPU. `native` = the CPU of the machine running `make` |
| `build` | `debug`, `release` | `debug` | `debug` adds logging (see below); `release` is smaller and silent |
| `part` | `1`..`4` | `1` | `build_bios` only: the MBR partition that becomes the **first menu entry**, so it boots automatically. The entries you added follow it |
| `disk` | a whole disk or image file, e.g. `/dev/sda` | not set | `build_bios` only: after building, install onto that disk; only this step runs with `sudo` (see [installing.md](installing.md)) |
| `yes` | `1` | not set | with `disk=`: do not ask for confirmation |

`ARCH=` and `BUILD=` (upper case) work too.

> **Write `arch=native`, not `-arch=native`.** GNU make treats every word that
> starts with `-` as one of its own options, so `make build_uefi -arch=native`
> fails with "invalid option". This cannot be changed from inside a Makefile.

`part=` is for `build_bios` only; with any other target it is an error. A `part=` build goes to its own directory (for example `build/bios/release-p2/`).

`arch=native` maps `uname -m` as follows: `x86_64` to `x86_64`,
`aarch64`/`arm64` to `aarch64`, `i386`..`i686` to `ia32`. The BIOS build is for
x86 only and refuses other architectures.

## Where the results are

| Command | Output |
|---|---|
| `make build_uefi arch=x86_64` | `build/x86_64/<build>/BOOTX64.EFI` |
| `make build_uefi arch=ia32` | `build/ia32/<build>/BOOTIA32.EFI` |
| `make build_uefi arch=aarch64` | `build/aarch64/<build>/BOOTAA64.EFI` |
| `make build_bios` | `build/bios/<build>/mbr.bin` and `stage2.bin` |

## debug and release

`debug` compiles in log messages. On UEFI they are printed on the console
(and on a serial console if the firmware mirrors it); on BIOS they go to the
serial port COM1 at 115200 baud. `release` removes them completely. The error
screen and the information screen exist in both builds.

## Changing the configuration

Edit `config/config.h` (settings) or `config/config.c` (boot entries) and run
the same `make` command again. Only the files that depend on what you changed
are rebuilt. See [configuration.md](configuration.md).

## Trying a build in QEMU

```
make run arch=x86_64        # also: ia32, aarch64
```

needs `qemu-system-*` and the matching firmware package (OVMF, OVMF IA32,
AAVMF). `tools/run-qemu.sh` shows the exact paths it expects.
