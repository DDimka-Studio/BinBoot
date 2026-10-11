# Troubleshooting

## The error screen

When a boot attempt fails, BinBoot stops and shows (colors aside):

```
Boot failed: Linux (UKI)
Cannot read the EFI file
/EFI/Linux/linux.efi
Error code: 0x800000000000000E

Press Enter to continue
```

Line 1 is the entry, line 2 what went wrong, line 3 the file (if any), line 4
the code. It stays until **Enter**, so you can photograph it. Then the menu
comes back with no countdown. `BB_ERROR_SCREEN 0` turns the screen off.

### UEFI error codes

The code is the standard UEFI status. The leading `8` (`0x8000...` on 64-bit,
`0x80000000...` on IA32) only marks "error"; the number after it is what counts.

| Code (low part) | Name | Typical cause here |
|---|---|---|
| `...01` | LOAD_ERROR | The file is not a valid EFI image |
| `...02` | INVALID_PARAMETER | Malformed `.volume` (PARTUUID), or a path longer than 255 characters |
| `...03` | UNSUPPORTED | The firmware cannot run the image, e.g. a 64-bit kernel on 32-bit firmware |
| `...07` | DEVICE_ERROR | The disk could not be read |
| `...0E` | NOT_FOUND | File or partition not found |
| `...0F` | ACCESS_DENIED | The firmware refused to read the file |
| `...1A` | SECURITY_VIOLATION | Secure Boot rejected the image (not signed or not enrolled) |

"... returned an error" means the program **did start** and then exited with an
error; its code is the program's own.

### UEFI messages

| Message | What to check |
|---|---|
| Cannot find or open the volume | `.volume` PARTUUID is wrong (compare with `lsblk -o NAME,PARTUUID`), or the partition has no filesystem the firmware reads |
| Cannot read the EFI file / kernel file / initrd file | The path is wrong. It starts at the root of the partition, not at `/boot`. FAT names are case-insensitive |
| Firmware refused to load ... | The file is not a valid image for this firmware, or Secure Boot is blocking it |
| The ... returned an error | The program ran and failed on its own |
| Cannot register the initrd with the firmware | The firmware refused to install the protocol that hands the initrd to the kernel; the code says why |
| No boot entries are configured / This entry has nothing to boot | `config.c` has an empty list, or an entry with no `.uki_path`, `.kernel` or `.image` |

### BIOS messages

| Message | Meaning |
|---|---|
| Cannot read the disk / boot sector (INT 13h) | BIOS disk error; the code is the INT 13h status (`01` bad command, `04` sector not found, `20` controller failure, `40` seek failure, `80` timeout) |
| The disk has no valid MBR / The boot sector is not valid (missing 55AA) | The target has no boot signature, so it cannot be started |
| The partition is empty or does not exist | `.partition` points at an unused slot |
| Invalid partition number (must be 0 to 4) | Fix `.partition` |
| Not a Linux kernel image (no setup header) | The sector list does not point at a bzImage; regenerate it with `tools/blocklist.py` |
| Kernel boot protocol is too old / Only bzImage kernels are supported | The kernel is too old or is a zImage |
| The initrd sector list does not match its size | `.initrd_size` is larger than the listed sectors |
| Cannot read the BIOS memory map / Not enough memory ... / Cannot enable the A20 address line | The machine could not provide what the Linux boot protocol needs |

## Common problems

**`'clang' not found`** - most often because the build was started with
`sudo make`: `sudo` resets `PATH`. Run `make` as a normal user (the BIOS install
step asks for root by itself). Otherwise install `clang` and `lld`, or pass
`CLANG=/path/to/clang` (see [building.md](building.md)).

**`make: invalid option -- 'a'`** - you wrote `-arch=native`. Write
`arch=native`: GNU make takes every word that starts with `-` as its own option.

**The screen is full of old text** or colors look wrong - set `BB_COLORS 0`.

**Pause/Break does nothing on the information screen** - your firmware does not
report that key. Set `BB_INFO_HOLD_CHAR` to an ordinary key, e.g. `'p'`.

**`[ FAIL ] Virtualization`** - the CPU supports VT-x/AMD-V but the firmware
setup has it switched off. Enable it there. This line is informational.

**An edit to `config.h` seems to be ignored** - run the same `make` command
again and check that it recompiled; if in doubt, `make clean`.

**A boot attempt works from the menu but not automatically** - nothing differs
between the two, but the automatic boot is cancelled by any key press,
including one made while the information screen is up.

**The BIOS install refuses the disk** - read the message: mounted, GPT, no MBR
signature or no free space before the first partition.
