#!/bin/sh
# Build a flashable .upt firmware image from a root filesystem tree.
#
#   ./build_image.sh --rootfs <squashfs-or-dir> --kernel <xImage> --out r1.upt
#
# The R1 updater (volume-up + power) reads r1.upt from the SD card, verifies
# every chunk against the md5 manifest, and writes the kernel to mtd1 and the
# root filesystem to mtd2 while running from the untouched recovery partition.
# That is the safe way to replace CompasOS: the recovery does the writing, so
# the kernel never erases the filesystem it is running from.
#
# Needs mksquashfs, split, md5sum, mkisofs (or genisoimage) and coreutils.
set -e

ROOTFS=""
KERNEL=""
OUT="r1.upt"
CHUNK=512k

usage() {
    sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --rootfs) ROOTFS="$2"; shift 2 ;;
        --kernel) KERNEL="$2"; shift 2 ;;
        --out)    OUT="$2"; shift 2 ;;
        --chunk)  CHUNK="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "unknown option: $1" >&2; usage ;;
    esac
done

[ -n "$ROOTFS" ] && [ -n "$KERNEL" ] || usage
[ -f "$KERNEL" ] || { echo "no such kernel: $KERNEL" >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/ota_v0"
cd "$WORK/ota_v0"

# A directory is packed here; a squashfs image is used as it is.
if [ -d "$ROOTFS" ]; then
    echo "== packing $ROOTFS"
    mksquashfs "$ROOTFS" rootfs.squashfs -comp lzo -b 131072 -no-progress -noappend >/dev/null
else
    cp "$ROOTFS" rootfs.squashfs
fi

rootfs_size=$(stat -c%s rootfs.squashfs)
rootfs_md5=$(md5sum rootfs.squashfs | awk '{print $1}')

# Chunks are named <image>.<index>.<md5 of the previous chunk>, the first one
# carrying the whole image's md5. The manifest lists each chunk's md5 in order,
# and is itself named after the whole image.
split rootfs.squashfs -d -a 4 -b "$CHUNK" rootfs.squashfs.
rm rootfs.squashfs
md5=$rootfs_md5
manifest="ota_md5_rootfs.squashfs.$rootfs_md5"
: > "$manifest"
for part in $(ls rootfs.squashfs.[0-9]* | sort); do
    next=$(md5sum "$part" | awk '{print $1}')
    echo "$next" >> "$manifest"
    mv "$part" "$part.$md5"
    md5=$next
done
echo "== rootfs: $rootfs_size bytes, md5 $rootfs_md5, $(wc -l < "$manifest") chunks"

cp "$KERNEL" xImage
ximage_size=$(stat -c%s xImage)
ximage_md5=$(md5sum xImage | awk '{print $1}')
split xImage -d -a 4 -b "$CHUNK" xImage.
rm xImage
md5=$ximage_md5
xman="ota_md5_xImage.$ximage_md5"
: > "$xman"
for part in $(ls xImage.[0-9]* | sort); do
    next=$(md5sum "$part" | awk '{print $1}')
    echo "$next" >> "$xman"
    mv "$part" "$part.$md5"
    md5=$next
done
echo "== xImage: $ximage_size bytes, md5 $ximage_md5"

cat > ota_update.in <<EOF
ota_version=0

img_type=kernel
img_name=xImage
img_size=$ximage_size
img_md5=$ximage_md5

img_type=rootfs
img_name=rootfs.squashfs
img_size=$rootfs_size
img_md5=$rootfs_md5
EOF
: > ota_v0.ok
echo "current_version=0" > ../ota_config.in

cd "$WORK"
OUT_ABS=$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")
# -r is what makes the long chunk names survive: without Rock Ridge the
# updater sees "rootfs.squashfs.0000...;1" and cannot find its chunks.
mkisofs -quiet -J -r -o "$OUT_ABS" "$WORK"
echo "== wrote $OUT_ABS"
ls -l "$OUT_ABS"
