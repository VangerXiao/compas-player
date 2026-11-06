#!/usr/bin/env python3
"""Rewrite a kernel module's vermagic so it loads on a kernel built elsewhere.

CompasOS ships one set of modules and SonixOS ships another, and the two kernels
report different vermagic strings ("4.4.94+" against "4.4.94-sonix"). A module
whose vermagic does not match the running kernel is refused by the loader even
when the code would work, because these kernels are built without
CONFIG_MODVERSIONS: vermagic is the only gate, and it is a string in the
.modinfo section.

This rewrites that string in place, keeping every other .modinfo entry intact.

    ./patch_module_vermagic.py foo.ko -o foo.ko.new --vermagic 4.4.94-sonix

Requires patch/objcopy (binutils) on PATH. The output is written beside the
input and can then be dropped into the root filesystem before packing.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile

# objcopy has to understand MIPS ELF; a host binutils built for x86 only will
# refuse the file with "Unable to recognise the format". Point OBJCOPY at the
# cross toolchain's objcopy when building on such a host.
OBJCOPY = os.environ.get("OBJCOPY", "objcopy")


def read_modinfo(path):
    tmpdir = tempfile.mkdtemp(prefix="vermagic-")
    out = os.path.join(tmpdir, "modinfo")
    subprocess.run([OBJCOPY, "--dump-section", ".modinfo=" + out, path],
                   check=True)
    with open(out, "rb") as f:
        data = f.read()
    shutil.rmtree(tmpdir)
    return data


def split_entries(data):
    """Split a .modinfo blob into its NUL-terminated key=value strings."""
    entries = []
    start = 0
    for i, byte in enumerate(data):
        if byte == 0:
            if i > start:
                entries.append(data[start:i])
            start = i + 1
    if start < len(data):
        entries.append(data[start:])
    return entries


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("module")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--vermagic", required=True,
                    help='target vermagic, e.g. "4.4.94-sonix"')
    args = ap.parse_args()

    data = read_modinfo(args.module)
    entries = split_entries(data)

    found = False
    out = []
    for entry in entries:
        if entry.startswith(b"vermagic="):
            # The entry is "vermagic=<version> <flags>": the kernel compares the
            # whole string, so the flags after the version must survive. Only the
            # version token is replaced here -- replacing the entire entry makes
            # the kernel reject every module with "version magic ... should be".
            value = entry[len(b"vermagic="):]
            marker = value.find(b" ")
            rest = value[marker:] if marker >= 0 else b""
            out.append(b"vermagic=" + args.vermagic.encode() + rest)
            found = True
        else:
            out.append(entry)
    if not found:
        sys.exit("%s: no vermagic entry in .modinfo" % args.module)

    blob = b"\0".join(out) + b"\0"

    shutil.copy2(args.module, args.output)
    tmpdir = tempfile.mkdtemp(prefix="vermagic-")
    section = os.path.join(tmpdir, "modinfo")
    with open(section, "wb") as f:
        f.write(blob)
    subprocess.run([OBJCOPY, "--update-section", ".modinfo=" + section,
                    args.output], check=True)
    shutil.rmtree(tmpdir)

    print("%s -> %s (vermagic=%s)" % (args.module, args.output, args.vermagic))


if __name__ == "__main__":
    main()
