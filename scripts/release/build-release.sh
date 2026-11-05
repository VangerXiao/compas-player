#!/bin/sh
# Build a flashable release from a root filesystem tree.
#
#   scripts/release/build-release.sh \
#       --rootfs <tree>            unpacked CompasOS root filesystem
#       --kernel <xImage>          kernel to ship in mtd1
#       --label  v1.4.1_Mutiboot   version string shown in About
#       --out    r1.upt            output image
#
# What it does:
#   1. stamps the label into a working copy of the source (the repository keeps
#      the neutral "Development Build" default, so the two release branches do
#      not fight over that line when they are rebased);
#   2. builds the player with `make target BOARD=r1`;
#   3. drops it into the root filesystem as /usr/bin/compas_player;
#   4. hands rootfs and kernel to packaging/dual-boot/build_image.sh, which
#      packs them into the .upt the recovery updater flashes.
#
# The MIPS musl toolchain has to be on PATH; scripts/tools/toolchain/
# install_mips_toolchain.sh fetches it.
set -eu

ROOTFS=""
KERNEL=""
LABEL=""
OUT="r1.upt"
BOARD="r1"

while [ $# -gt 0 ]; do
    case "$1" in
        --rootfs) ROOTFS="$2"; shift 2 ;;
        --kernel) KERNEL="$2"; shift 2 ;;
        --label)  LABEL="$2"; shift 2 ;;
        --out)    OUT="$2"; shift 2 ;;
        --board)  BOARD="$2"; shift 2 ;;
        -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

[ -n "$ROOTFS" ] && [ -n "$KERNEL" ] && [ -n "$LABEL" ] || {
    echo "usage: $0 --rootfs <tree> --kernel <xImage> --label <label> [--out r1.upt]" >&2
    exit 2
}
[ -d "$ROOTFS" ] && [ -f "$KERNEL" ] || { echo "rootfs or kernel not found" >&2; exit 1; }

REPO=$(cd "$(dirname "$0")/../.." && pwd)
cd "$REPO"

echo "== dependencies"
scripts/release/fetch-deps.sh

echo "== stamping label $LABEL"
sed -i "s|#define APP_RELEASE_LABEL \"Development Build\"|#define APP_RELEASE_LABEL \"$LABEL\"|" \
    src/core/system/runtime/app_version.c
grep -n 'APP_RELEASE_LABEL "' src/core/system/runtime/app_version.c | head -1

echo "== building player"
make target BOARD="$BOARD" -j"$(nproc 2>/dev/null || echo 4)"

echo "== installing player into the root filesystem"
install -m 0755 build_target/compas_player_target "$ROOTFS/usr/bin/compas_player"

echo "== packing $OUT"
packaging/dual-boot/build_image.sh --rootfs "$ROOTFS" --kernel "$KERNEL" --out "$OUT"

echo "== done: $OUT"
