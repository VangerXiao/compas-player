#!/usr/bin/env python3
"""Let SonixOS keep its data on the card.

The hook binds <card>/sonix/data over /usr/data before the chroot, but SonixOS's
own init would then mount the UBI volume straight over that bind. Its guard asks
for the stricter " /usr/data ubifs ", so nothing that is already mounted at that
path counts. Both places where it mounts need the looser test:

  etc/init.d/S21mount_ubifs        the normal startup mount
  etc/init.d/S11jpeg_display_shell the early mount made for a custom boot logo

Usage: patch_sonix_ubifs.py <unsquashed-sonix-rootfs>

Safe to run twice: a tree that already carries the change is left alone.
"""
import io
import sys

S21 = "/etc/init.d/S21mount_ubifs"
S11 = "/etc/init.d/S11jpeg_display_shell"

STRICT = '" /usr/data ubifs "'
LOOSE = '" /usr/data "'


def patch(tree, rel, replacements):
    path = tree + rel
    try:
        text = io.open(path, encoding="utf-8").read()
    except FileNotFoundError:
        print("missing:", path)
        return False
    original = text
    for old, new in replacements:
        text = text.replace(old, new)
    if text == original:
        print("already patched:", rel)
        return True
    io.open(path, "w", encoding="utf-8").write(text)
    print("patched:", rel)
    return True


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    tree = sys.argv[1].rstrip("/")

    ok = patch(tree, S21, [(STRICT, LOOSE)])

    # S11's early mount also has to stand down when the data directory is
    # already a mount, or it would replace the bind with the UBI volume.
    ok &= patch(tree, S11, [
        ('if [ "$custom" = 1 ]; then',
         'if [ "$custom" = 1 ] && ! grep -q " /usr/data " /proc/mounts; then'),
    ])
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
