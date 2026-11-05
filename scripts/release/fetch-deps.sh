#!/bin/sh
# Clone the third-party checkouts the Makefile pulls in by itself.
#
# The Makefile runs its own fetch/clone for each dependency and never checks the
# exit status, so an interrupted or throttled connection leaves an empty
# directory behind and the build then fails much later with a confusing
# "lua.h: No such file or directory". Running this first makes that failure
# visible where it happens.
#
#   scripts/release/fetch-deps.sh [--force]
#
# --force re-fetches even the directories that already exist.
set -eu

FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

cd "$(dirname "$0")/../.." || exit 1
echo "== working in $PWD"

# name|url|branch
DEPS='
lvgl|https://github.com/lvgl/lvgl.git|v9.5.0
dr_libs|https://github.com/mackron/dr_libs.git|
tinyalsa|https://github.com/tinyalsa/tinyalsa.git|
faad2|https://github.com/knik0/faad2.git|
alac|https://github.com/mikebrady/alac.git|
opus|https://github.com/xiph/opus.git|v1.5.2
libogg|https://github.com/xiph/ogg.git|v1.3.5
opusfile|https://github.com/xiph/opusfile.git|v0.12
jpeg|https://github.com/libjpeg-turbo/ijg.git|jpeg-9f
mbedtls|https://github.com/Mbed-TLS/mbedtls.git|v3.6.2
cJSON|https://github.com/DaveGamble/cJSON.git|
dbus|https://gitlab.freedesktop.org/dbus/dbus.git|dbus-1.16.2
libexecinfo|https://github.com/fam007e/libexecinfo.git|
'

fail=0
for entry in $DEPS; do
    name=$(echo "$entry" | cut -d'|' -f1)
    url=$(echo "$entry" | cut -d'|' -f2)
    branch=$(echo "$entry" | cut -d'|' -f3)

    if [ -d "$name" ] && [ "$FORCE" -eq 0 ]; then
        if [ -n "$(ls -A "$name" 2>/dev/null)" ]; then
            echo "  ok      $name"
            continue
        fi
        rm -rf "$name"
    fi

    echo "  clone   $name"
    ok=0
    for try in 1 2 3 4 5; do
        rm -rf "$name"
        if [ -n "$branch" ]; then
            git clone --quiet --depth 1 -b "$branch" "$url" "$name" && ok=1
        else
            git clone --quiet --depth 1 "$url" "$name" && ok=1
        fi
        [ "$ok" -eq 1 ] && break
        echo "          retry $try"
        sleep 3
    done
    if [ "$ok" -eq 0 ]; then
        echo "  FAILED  $name ($url)"
        fail=1
    fi
done

[ "$fail" -eq 0 ] || { echo "== some dependencies are missing"; exit 1; }

# Lua is not a git clone: the Makefile fetches the release tarball from lua.org
# and extracts it with the sources in lua/src, which is what the include paths
# and the source list expect. A clone of github.com/lua/lua has them at the top
# level instead and will not build.
LUA_VER=5.5.1
if [ -f lua/src/lua.h ] && [ "$FORCE" -eq 0 ]; then
    echo "  ok      lua"
else
    echo "  fetch   lua-$LUA_VER"
    ok=0
    for try in 1 2 3 4 5; do
        rm -rf lua
        if curl -fsSL -o "/tmp/lua-$LUA_VER.tar.gz" "https://www.lua.org/ftp/lua-$LUA_VER.tar.gz" &&
            tar -xzf "/tmp/lua-$LUA_VER.tar.gz" -C /tmp &&
            mv "/tmp/lua-$LUA_VER" lua; then
            ok=1
            rm -f "/tmp/lua-$LUA_VER.tar.gz"
            break
        fi
        echo "          retry $try"
        sleep 3
    done
    [ "$ok" -eq 1 ] || { echo "  FAILED  lua ($LUA_VER)"; exit 1; }
fi

echo "== all dependencies present"
