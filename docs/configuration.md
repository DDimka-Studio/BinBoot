# Configuration

Two files hold everything:

- `config/config.h` - **settings**: timeouts, texts, colors, on/off switches.
- `config/config.c` - **boot entries**: what the menu offers.

Edit, then run the same `make` command again. Nothing is read at boot time.

## Boot entries (`config/config.c`)

The file contains one list per firmware: UEFI and Legacy BIOS. Only the list
for the firmware you build for is used. An entry is a `{ ... }` block. The menu
shows the blocks in order, and `BB_DEFAULT_ENTRY` counts along the same order
starting from 0.

- **Add**: copy a template from the comment at the top of the list into the
  list, put a comma after the closing brace, change the values.
- **Remove**: delete the block or wrap it in a comment.
- **Reorder**: move the blocks; remember `BB_DEFAULT_ENTRY`.

Anything you leave out of a block is "not set". All strings are plain ASCII.

### Fields

| Field | Firmware | Meaning |
|---|---|---|
| `.name` | all | Text shown in the menu |
| `.uki_path` | UEFI | Path of an EFI program on a disk: a UKI, `bootmgfw.efi`, another loader |
| `.volume` | UEFI | Which partition `.uki_path`, `.kernel` and `.initrd` are read from (PARTUUID). Not set = the partition BinBoot was loaded from |
| `.kernel`, `.initrd`, `.cmdline` | UEFI | A Linux kernel with an EFI stub, its initrd (optional) and command line (optional) |
| `.image` | UEFI | An EFI program stored inside the BinBoot file (`&name` of a `BB_EMBED`) |
| `.chain` | BIOS | Start the boot sector of a partition or disk |
| `.bios_linux` | BIOS | Start Linux from raw sectors; uses `.cmdline` |

If an entry has several of `.image`, `.uki_path`, `.kernel`, the first one in
that order is used (UEFI).

### Paths

Paths start at the **root of the chosen partition**, with forward slashes. If
that partition is mounted at `/boot` on your system, the file `/boot/vmlinuz`
is `"/vmlinuz"` in the config. Names are case-insensitive (FAT).

### Choosing a disk: `.volume`

`.volume` is a Linux-style PARTUUID:

| Disk | Format | Example |
|---|---|---|
| GPT | partition GUID | `"6f1c2a3b-4d5e-4f60-8a7b-9c0d1e2f3a4b"` |
| MBR | disk signature, dash, partition number (hex, 2 digits) | `"12345678-01"` |

Find it with `lsblk -o NAME,PARTUUID` (or `blkid`). Upper or lower case both
work. If no partition matches, the error screen says so (error code
`...0E`, "not found"). The partition must have a filesystem the firmware can
read, normally FAT.

### Examples (UEFI)

A UKI on another disk:

```c
{
    .name     = "Arch (UKI)",
    .uki_path = "/EFI/Linux/arch.efi",
    .volume   = "6f1c2a3b-4d5e-4f60-8a7b-9c0d1e2f3a4b",
},
```

Windows, from the ESP BinBoot sits on:

```c
{
    .name     = "Windows",
    .uki_path = "/EFI/Microsoft/Boot/bootmgfw.efi",
},
```

A Linux kernel with an EFI stub:

```c
{
    .name    = "Linux",
    .kernel  = "/vmlinuz-linux",
    .initrd  = "/initramfs-linux.img",
    .cmdline = "root=/dev/sda2 rw quiet",
},
```

An EFI program inside the loader (the file then needs nothing else on disk).
Declare it once above the list, with the file under `blobs/<arch>/` (create that folder yourself, see [embedding.md](embedding.md)):

```c
BB_EMBED(blob_rescue, "blobs/" BB_ARCH "/rescue.efi");
...
{ .name = "Rescue", .image = &blob_rescue },
```

`BB_ARCH` becomes `x86_64`, `ia32` or `aarch64`, so each architecture has its
own file. The BinBoot file grows by the size of the embedded program. Changing
a file under `blobs/` triggers a rebuild automatically.

### Examples (Legacy BIOS)

**The first BIOS entry is generated.** `make build_bios part=N` makes partition
N of the boot disk the first entry, which is also the one that boots
automatically (without `part=`, partition 1). Entries you add below it keep
their order and simply follow, so adding or changing `part=` shifts them down
or up. Its name is `BB_PART_ENTRY_PREFIX` plus the number ("Partition 2"). To
get rid of it, delete its block in `config.c`; then `part=` has no effect. If
you also list the same partition yourself, the menu shows it twice, so leave it
out of your own entries.

Start the boot sector of partition 1 of the disk BinBoot was loaded from:

```c
{
    .name  = "Windows",
    .chain = { .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE, .partition = 1 },
},
```

`.drive` is a BIOS number (`0x80` first disk, `0x81` second) or
`BB_BIOS_BOOT_DRIVE`. `.partition` is 1..4 (MBR primary partitions), or 0 for
the disk's own MBR.

Linux from raw sectors (no filesystem driver): the kernel (a bzImage with boot
protocol 2.06 or newer) and the initrd are given as lists of disk sectors.

```
sudo tools/blocklist.py /boot/vmlinuz-6.8.0 --name kernel
sudo tools/blocklist.py /boot/initrd.img-6.8.0 --name initrd
```

prints `struct bb_extent` arrays to paste above the list. Then:

```c
{
    .name    = "Linux",
    .cmdline = "root=/dev/sda2 rw quiet",
    .bios_linux = {
        .enabled = 1, .drive = BB_BIOS_BOOT_DRIVE,
        .kernel = kernel_ext, .kernel_count = BB_COUNT(kernel_ext),
        .initrd = initrd_ext, .initrd_count = BB_COUNT(initrd_ext),
        .initrd_size = 339000,          /* exact bytes, printed by the tool */
    },
},
```

The sector lists are valid only while the files stay where they are: after
every kernel or initrd update, run the tool again and rebuild. The files must
be on a plain partition (no LVM, no RAID) of the boot disk, on a filesystem
that supports FIEMAP (ext4, xfs, ...). Disks larger than 2 TiB are not
supported. The tool prints a `dd | cmp` command so you can check the sectors.

## Settings (`config/config.h`)

### 1. Menu and automatic boot

| Option | Default | Meaning |
|---|---|---|
| `BB_DEFAULT_ENTRY` | `0` | Entry selected at start (position in `config.c`, from 0) |
| `BB_TIMEOUT_SECONDS` | `5` | Seconds until the selected entry boots. Any key cancels. `0` = boot at once, no menu |
| `BB_TIMEOUT_PREFIX`, `BB_TIMEOUT_SUFFIX` | `"Booting in "`, `" s"` | Text of the countdown on the bottom row |
| `BB_PART_ENTRY_PREFIX` | `"Partition "` | BIOS: name of the generated first entry (followed by the partition number) |

Menu keys: Up / Down move the `>` marker, Enter boots the selected entry.

### 2. Information screen (UEFI)

A screen of its own, shown before the menu and replaced by it:

```
[  OK  ] Memory: 15.7 GiB
[  OK  ] CPU: Intel(R) Core(TM) ...
[  OK  ] Features: SSE2 SSE4.2 AES AVX AVX2 NX x86-64
[  OK  ] Virtualization: Intel VT-x, EPT, unrestricted guest
[  OK  ] Video adapter: Intel [8086:5916]
[  OK  ] Display: 1920x1080 (graphics output)
```

| Option | Default | Meaning |
|---|---|---|
| `BB_BOOT_INFO` | `1` | `0` turns the whole screen off |
| `BB_INFO_SECONDS` | `2` | Seconds it stays up; `0` = not shown. Any key skips it **and** cancels the menu's automatic boot |
| `BB_INFO_HOLD_CHAR` | `0` | Besides Pause/Break (which freezes the screen until the next key; not every firmware reports it), this character does the same, e.g. `'p'`. `0` = none |
| `BB_INFO_MEMORY`, `_CPU`, `_FEATURES`, `_VIRT`, `_VIDEO` | `1` | One switch per line |
| `BB_INFO_TAG_OK`, `_WARN`, `_FAIL` | `"[  OK  ]"`... | Text of the status tags; keep them equally wide |
| `BB_INFO_LABEL_*` | `"Memory: "`... | Text in front of each value |

What the lines mean:

- **Memory**: RAM the OS can use, from the firmware memory map (reserved ranges
  are excluded, so 16 GiB installed usually shows as a little less).
- **Virtualization** (x86): `[ OK ]` with the technology and, on Intel, whether
  EPT and unrestricted guest exist; `[ FAIL ]` if the CPU has VT-x/AMD-V but
  the firmware setup switched it off; `[ WARN ]` if the CPU has none. On
  AArch64 it reports the exception level BinBoot runs at (EL2 means a
  hypervisor level is available).
- **Video adapter**: the first PCI display controller, by vendor and
  `vendor:device` ids. It may be absent on boards without PCI video (for
  example the Raspberry Pi), which gives `[ WARN ]`.

### 3. Colors

`BB_COLORS` (`1`/`0`), then `BB_COLOR_TEXT_FG`, `_TEXT_BG`, `_INFO_OK`,
`_INFO_WARN`, `_INFO_FAIL`, `_ERROR_TITLE`. Values: 0 black, 1 blue, 2 green,
3 cyan, 4 red, 5 magenta, 6 brown, 7 light gray, 8 dark gray, 9 light blue,
10 light green, 11 light cyan, 12 light red, 13 light magenta, 14 yellow,
15 white. Backgrounds use 0..7. Works in UEFI and BIOS text mode.

### 4. Error screen

| Option | Default | Meaning |
|---|---|---|
| `BB_ERROR_SCREEN` | `1` | `0` = skip the screen, go straight back to the menu |
| `BB_ERROR_TITLE`, `BB_ERROR_CODE_LABEL`, `BB_ERROR_PROMPT` | `"Boot failed"`... | Its texts |

When a boot attempt fails the screen names the entry, what went wrong, the
file if there is one and the error code, and waits for Enter (other keys are
ignored, keys pressed earlier are dropped). Then the menu returns **without**
the countdown, so a failing default entry cannot loop. See
[troubleshooting.md](troubleshooting.md).
