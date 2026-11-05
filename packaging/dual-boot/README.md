# Dual boot with SonixOS

CompasOS stays the main system. SonixOS is a second, self-contained player that
boots from the SD card when you ask for it, and hands the device back when you
are done. Nothing in the recovery partitions is touched, so **volume-up + power
still reflashes `r1.upt` from the card** exactly as before.

```
power on
   |
   +-- CompasOS (mtd1 kernel, mtd2 rootfs)   <- always boots by default
   |      |
   |      +-- Settings -> System -> "Reboot to SonixOS"
   |              writes /sonix/boot on the card, reboots
   |
   +-- S20sonix hook (runs before the CompasOS userland mounts /usr/data)
          |
          +-- /sonix/boot present?  mount /sonix/rootfs.squashfs,
                                     bind-mount /sonix/data over /usr/data,
                                     chroot into SonixOS
                        |
                        +-- power menu -> "Slide to reboot to CompasOS"
                                writes /usr/data/boot_compas, reboots
                                (hook clears both files, CompasOS comes up)
```

## What goes where

| Path on the card | Purpose |
|---|---|
| `/sonix/rootfs.squashfs` | SonixOS root filesystem (the whole system, ~45 MB) |
| `/sonix/data/` | SonixOS's own `/usr/data` - its settings, database, covers |
| `/sonix/boot` | Marker: present = boot SonixOS next time |
| `/sonix/data/boot_compas` | Flag written by SonixOS: present = go back to CompasOS |
| `/r1.upt` | Recovery image for volume-up flashing (keep one at all times) |

SonixOS's data lives on the card, not in the internal UBI volume, so a SonixOS
session can never wipe CompasOS's settings (`/usr/data` is a different thing
entirely once the hook has mounted the card there).

## Building

1. Build CompasOS with the dual-boot row (`src/ui/settings/gui_settings.c`, already
   in this tree) and copy `S20sonix` into the root filesystem as
   `/etc/init.d/S20sonix` (mode 0755).

2. Patch the module set to the kernel you are going to boot. Both players ship
   their own kernel and their own modules, and the two kernels carry different
   vermagic strings. The kernels are built without `CONFIG_MODVERSIONS`, so
   vermagic is the only gate and rewriting it is enough:

   ```sh
   for ko in rootfs/lib/modules/*.ko; do
       packaging/dual-boot/patch_module_vermagic.py "$ko" -o "$ko.new" \
           --vermagic 4.4.94-sonix
       mv "$ko.new" "$ko"
   done
   ```

   (`4.4.94-sonix` matches the SonixOS kernel this project boots. Use `4.4.94+`
   for the stock CompasOS kernel.)

3. Pack and flash:

   ```sh
   packaging/dual-boot/build_image.sh --rootfs ./rootfs \
       --kernel xImage-R1 --out r1.upt
   cp r1.upt /media/card/          # card root, name must be r1.upt
   ```

   Then on the device: hold volume-up, press power, keep holding until the
   upgrade screen appears. The recovery writes mtd1/mtd2 and reboots.

## Notes

- The hook runs at S20, before `S21mount_ubifs`, so the internal `userdata`
  volume is never mounted while SonixOS is in charge.
- SonixOS's own `S11`/`S21` scripts are guarded with
  `grep -q " /usr/data " /proc/mounts ||` so they leave the bind mount alone.
- `switch_root` cannot be used here: the SonixOS root is a read-only squashfs.
  `chroot` works and is what the hook does.
