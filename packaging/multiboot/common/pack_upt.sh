#!/bin/bash
# Pack an arbitrary rootfs tree with an arbitrary kernel into a .upt.
#   pack_any.sh --base <tree> --kernel <xImage> --out <name.upt>
set -e
BASE=""; KERNEL=""; OUT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --base) BASE="$2"; shift 2 ;;
        --kernel) KERNEL="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        *) echo "unknown $1"; exit 2 ;;
    esac
done
[ -d "$BASE" ] && [ -f "$KERNEL" ] && [ -n "$OUT" ] || { echo "usage: $0 --base <tree> --kernel <xImage> --out <name.upt>"; exit 2; }

OUTDIR=/r1fw/out
NAME="${OUT%.upt}"
ROOTFS="$OUTDIR/rootfs_$NAME.squashfs"
echo "== base: $BASE"
echo "== kernel: $KERNEL ($(stat -c%s "$KERNEL") bytes)"
echo "   kernel vermagic hint: $(strings "$KERNEL" | grep -m1 -o '4\.4\.94[^ ]*' || echo '?')"
echo "   modules: $(ls "$BASE"/module_driver/*.ko 2>/dev/null | wc -l), vermagic: $(strings "$BASE"/module_driver/lcd_lg35583.ko 2>/dev/null | grep -m1 -o 'vermagic=[^ ]*' || echo '?')"

rm -f "$ROOTFS"
mksquashfs "$BASE" "$ROOTFS" -comp lzo -b 131072 -no-progress -noappend >/dev/null 2>&1
echo "== squashfs: $(stat -c%s "$ROOTFS") bytes"

WORK=/tmp/upt_$NAME
rm -rf "$WORK"; mkdir -p "$WORK/ota_v0"; cd "$WORK/ota_v0"
cp "$ROOTFS" rootfs.squashfs
rsize=$(stat -c%s rootfs.squashfs); rmd5=$(md5sum rootfs.squashfs | awk '{print $1}')
split rootfs.squashfs -d -a 4 -b 512k rootfs.squashfs.; rm rootfs.squashfs
md5=$rmd5; : > "ota_md5_rootfs.squashfs.$rmd5"
for p in $(ls rootfs.squashfs.[0-9]* | sort); do n=$(md5sum "$p" | awk '{print $1}'); echo "$n" >> "ota_md5_rootfs.squashfs.$rmd5"; mv "$p" "$p.$md5"; md5=$n; done

cp "$KERNEL" xImage
ksize=$(stat -c%s xImage); kmd5=$(md5sum xImage | awk '{print $1}')
split xImage -d -a 4 -b 512k xImage.; rm xImage
md5=$kmd5; : > "ota_md5_xImage.$kmd5"
for p in $(ls xImage.[0-9]* | sort); do n=$(md5sum "$p" | awk '{print $1}'); echo "$n" >> "ota_md5_xImage.$kmd5"; mv "$p" "$p.$md5"; md5=$n; done

cat > ota_update.in <<EOF
ota_version=0

img_type=kernel
img_name=xImage
img_size=$ksize
img_md5=$kmd5

img_type=rootfs
img_name=rootfs.squashfs
img_size=$rsize
img_md5=$rmd5
EOF
: > ota_v0.ok
echo "current_version=0" > ../ota_config.in
cd "$WORK"
rm -f "$OUTDIR/$OUT"
mkisofs -quiet -J -r -o "$OUTDIR/$OUT" "$WORK" >/dev/null 2>&1
echo "== done: $OUTDIR/$OUT"
ls -l "$OUTDIR/$OUT"
sha256sum "$OUTDIR/$OUT"
