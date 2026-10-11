# Installing

## UEFI (x86_64, IA32, AArch64)

BinBoot is an ordinary EFI program, so installing it means putting the file on
an EFI System Partition (ESP, a FAT partition) and letting the firmware find it.

1. Mount the ESP, for example at `/boot/efi`.
2. Copy the file. The **fallback path** needs no registration:

   | Build | Copy to |
   |---|---|
   | x86_64 | `EFI/BOOT/BOOTX64.EFI` |
   | IA32 (32-bit firmware) | `EFI/BOOT/BOOTIA32.EFI` |
   | AArch64 | `EFI/BOOT/BOOTAA64.EFI` |

   Careful: that path may already hold another loader (Windows and some
   distributions use it). Keep a copy.

3. Or register it as a normal boot entry and put it first:

   ```
   sudo mkdir -p /boot/efi/EFI/BinBoot
   sudo cp build/x86_64/release/BOOTX64.EFI /boot/efi/EFI/BinBoot/
   sudo efibootmgr --create --disk /dev/nvme0n1 --part 1 \
        --loader '\EFI\BinBoot\BOOTX64.EFI' --label BinBoot
   ```

   `efibootmgr` prints the new entry's number; `sudo efibootmgr -o 0003,0001`
   (your numbers) sets the boot order. Leave your previous entry in the list
   as a fallback.

With the default `BB_TIMEOUT_SECONDS 5` the default entry boots after five
seconds and any key opens the menu, so the firmware's own boot menu key is not
needed.

**32-bit firmware on a 64-bit CPU** (some Atom tablets, early Macs) only loads
`BOOTIA32.EFI`. See the limits in [README.md](README.md): 64-bit Linux kernels
cannot be started from it yet, 32-bit EFI programs can.

**Raspberry Pi 4**: BinBoot is an EFI program, so it needs a UEFI firmware on
the board first; this combination has not been tested.

**Secure Boot**: BinBoot is not signed. With Secure Boot enabled the firmware
will refuse it (and the images it starts must be signed too). Not tested.

## Legacy BIOS

The simple way: build and install in one command (it asks before writing):

```
make build_bios build=release part=1 disk=/dev/sdX    # the WHOLE disk, not /dev/sdX1
```

`disk=` is the disk BinBoot is written to (BIOS only ever starts the first
sector of a whole disk). `part=` is the partition on it that the menu boots
first and by default, for example the partition with your system; it must
exist, otherwise the install stops before writing anything. Run it as a normal
user: the build needs your environment (your compiler), and `make` runs only
the write itself with `sudo`. Without `part=` it
is partition 1. Entries you added in `config.c` follow it. `yes=1` skips the
question. The same thing in two steps:

```
make build_bios build=release
sudo tools/install-bios.sh /dev/sdX
```

Options: `--yes` (do not ask), `--force` (allow a blank disk), `--build DIR`
(default `build/bios/release`; use `build/bios/debug` for a debug build),
`--backup FILE` (where the old first sector is saved). The target can also be
a disk image file.

What it writes:

- the boot code into the first 440 bytes of the MBR (the disk signature and the
  partition table that follow are **not** touched);
- the second stage (about 7 KiB) into the sectors right after the MBR.

The script refuses to continue when: the target is a partition (give the whole
disk), the target is mounted, the disk is GPT
(BIOS boot from GPT is not supported), the first sector is not a valid MBR
(unless `--force`), or there is not enough free space between the MBR and the
first partition. Modern partitioning tools start the first partition at sector
2048, which leaves plenty.

It saves the original first sector as `bb-mbr-backup-<disk>.bin` in the current
directory before writing. To undo the installation:

```
sudo dd if=bb-mbr-backup-sdX.bin of=/dev/sdX bs=440 count=1 conv=notrunc
```

This restores the old boot code; the few sectors of the second stage are left
behind and are harmless.

The BIOS build needs INT 13h extensions (every BIOS since the late 1990s) and
reads MBR primary partitions 1 to 4.
