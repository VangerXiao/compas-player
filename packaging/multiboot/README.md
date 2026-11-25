# Multi-boot packaging

Everything needed to turn a stock HiBy firmware into a multi-boot image lives
here. The player source itself is device-agnostic; the parts below are what
differs per device.

```
packaging/multiboot/
  common/          pack_upt.sh, patch_modules.sh, patch_module_vermagic.py
  r1/              S20switch, bootmenu.c, gen_bootmenu.py, assets/, S99shutdown_logo
  r3ii_2025/       S20switch, bootmenu.c, gen_bootmenu.py, assets/
```

## How a boot works

`kernel -> /linuxrc -> rcS`. The hook is installed as `S20switch`, so it runs
after the display and module scripts but before `S21mount_ubifs`:

1. wait briefly for the card, mount it at `/tmp/sd`;
2. read `/tmp/sd/boot_target.txt` (`stock`, `compas`, `sonix`, and `rockbox` on
   the R1);
3. mount the target's `rootfs.squashfs` from the card and `chroot` into it;
4. give the target its user data (see the table below), move the card to
   `<data>/mnt/sd_0` so the player finds it, and hand over `/dev`, `/proc`, `/sys`.

Every failure path (`exit 0`) continues the stock boot, so a missing card, a
missing image, an unparsable marker or a broken hook can never leave the device
without a system. A file named `safe` in the card root forces the stock system,
and a file named `recovery` writes the `kernel2` flag and reboots into the
recovery system, which can reflash from the card.

## Where the user data lives

| System | Root | User data | Why |
|---|---|---|---|
| HiByOS | internal rootfs | internal UBI | stock behaviour |
| CompásOS | card squashfs | internal UBI | it is a HiByOS fork: same `/usr/data` layout, and its init insists on a UBIFS volume |
| SonixOS | card squashfs | card `sonix/data` | different lineage; its mount helper can erase and re-create the UBI volume when it does not recognise the layout, which is how settings used to be lost when switching |
| Rockbox (R1) | card `.rockbox` | card | Rockbox keeps its own files |

The R1 hook binds `sonix/data` over `/usr/data`; that needs Sonix's
`S21mount_ubifs` to accept *any* mount at that path (upstream tests for
`" /usr/data ubifs "`). The R3II image carries the same one-line relaxation,
plus the same check in `S11jpeg_display_shell`.

## Building an image

```sh
# 1. unpack the stock .upt, keep its rootfs tree as the base
python3 extract_upt_rootfs.py stock.upt stock.squashfs && unsquashfs stock.squashfs base/

# 2. install the hook (and, on the R1, the menu and the shutdown logo)
install -m 0755 packaging/multiboot/<device>/S20switch base/etc/init.d/S20switch

# 3. compile the menu and render its frames (see the device's gen_bootmenu.py)
mipsel-linux-musl-gcc -O2 -static -o bootmenu packaging/multiboot/<device>/bootmenu.c
python3 packaging/multiboot/<device>/gen_bootmenu.py out/ <cjk.ttf> ...

# 4. pack
packaging/multiboot/common/pack_upt.sh --base base --kernel xImage --out out.upt
```

`pack_upt.sh` writes the same layout the stock updater expects: 512 KiB chunks
named `<image>.<index>.<md5>` with an md5 chain, `ota_update.in`, `ota_config.in`
and an ISO9660 container.

## Devices

See `..//r1/device.conf` and `../r3ii_2025/device.conf` for partition tables,
kernel identities and the target list.

## Testing an image without a PC

Put the `.upt` in the card root and use **Settings -> Firmware update -> via SD
card**. If the system no longer boots, the recovery system still does: create a
file named `recovery` in the card root before powering on, or reflash with the
vendor cloner.

[![](https://img.shields.io/badge/powered_by-dsh-4D6BFE?style=flat-square&logo=deepseek&logoColor=white)](https://github.com/deepseek-ai/deepseek-harness)
