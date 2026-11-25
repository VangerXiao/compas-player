#!/bin/bash
# Rewrite every module's vermagic to the kernel the image actually ships.
#   patch_modules.sh <rootfs-tree> <vermagic>
# Honours OBJCOPY (a host binutils cannot read MIPS ELF; point it at the cross
# toolchain's objcopy).
set -e
TREE="$1"
VM="$2"
[ -d "$TREE" ] && [ -n "$VM" ] || { echo "usage: $0 <tree> <vermagic>"; exit 2; }

PY=${PATCH_MODULE_PY:-/r1fw/work/packaging/patch_module_vermagic.py}
[ -f "$PY" ] || PY=/build/cp15-mb/packaging/dual-boot/patch_module_vermagic.py
echo "using $PY"

n=0
for ko in "$TREE"/module_driver/*.ko; do
    [ -f "$ko" ] || continue
    python3 "$PY" "$ko" -o "$ko.new" --vermagic "$VM" >/dev/null
    mv "$ko.new" "$ko"
    n=$((n + 1))
done
echo "patched $n modules"

# Report the distinct vermagic values actually present.
echo "=== vermagic summary"
for ko in "$TREE"/module_driver/*.ko; do
    [ -f "$ko" ] || continue
    strings "$ko" | grep -m1 'vermagic=' | sed 's/ preempt.*//'
done | sort | uniq -c
