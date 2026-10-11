# Testing

All tests run the real build in QEMU with real firmware images and check what
appears on the (serial) console or in the VGA text buffer. They prove that the
logic works on a correct firmware; they do **not** prove behaviour on real
hardware, with Secure Boot, on a Raspberry Pi, or for VT-x/EPT detection on
Intel CPUs (QEMU does not offer VMX).

## What to install

`qemu-system-x86`, `qemu-system-arm`, `ovmf`, `ovmf-ia32`, `qemu-efi-aarch64`,
`dosfstools`, `mtools`, `python3`, `gcc` (for a static test `/init`), plus
`gdisk` if you want to inspect the generated GPT images. The Linux tests also
need a kernel with an EFI stub and a BIOS-bootable bzImage, for example an
Ubuntu `vmlinuz`.

## The scripts

| Command | What it checks |
|---|---|
| `make bios-test` | BIOS: install into a disk image, menu, countdown, key navigation, chainloading two partitions, error screen for an empty partition |
| `tests/bios/run-part.sh` | BIOS: `make build_bios part=N disk=...` end to end: the right partition boots by default, other entries shift down, an empty partition is refused and the disk stays untouched |
| `tests/bios/run-linux.sh KERNEL` | BIOS: a real kernel and initrd stored in raw sectors boot; an invalid kernel shows the error screen |
| `make linux-test KERNEL=...` | UEFI x86_64: kernel + initrd + command line read from the ESP and started |
| `tests/uefi/run-uki.sh gpt\|mbr\|missing` | UEFI: an EFI file read from a **second disk** chosen by PARTUUID (GPT, MBR); a wrong PARTUUID gives the error screen |
| `tests/uefi/run-ia32.sh embedded\|file` | 32-bit UEFI: embedded image and EFI file from disk |
| `tests/uefi/run-error.sh` | UEFI: error screen stays until Enter, ignores other keys, menu returns without a countdown |
| `tests/uefi/run-info.sh` | UEFI: information screen shown on its own and replaced by the menu, key skip, hold key, color codes |
| `tests/uefi/screen.py LOG` | Helper: rebuilds the final text screen from a QEMU serial log |

Some tests are slow (a kernel boots under emulation in about a minute).
Boot-time-dependent tests wait for text in the log rather than fixed delays,
but a heavily loaded machine can still make a run time out; just run it again.

## Not covered

Secure Boot, real hardware of any kind, Raspberry Pi 4, a 64-bit kernel on
32-bit firmware (known to fail, see [README.md](README.md)), the VT-x branch of
the information screen, Pause/Break (QEMU's serial console cannot send it),
very large initrds, and disks other than the layouts the test scripts create.
