# Release scripts

Two things this fork needs on top of upstream's own build:

| Script | Purpose |
|---|---|
| `fetch-deps.sh` | clone the third-party checkouts the Makefile pulls in by itself, with retries |
| `build-release.sh` | stamp the version label, build the player, install it into a root filesystem and pack the `.upt` |

## Why `fetch-deps.sh` exists

The Makefile clones LVGL, opus, faad2, dbus and friends with
`$(shell git clone ...)`. Make never checks the exit status of a `$(shell ...)`
call, so a dropped connection leaves an empty directory and the failure only
surfaces much later — as a missing header in an unrelated file, or as LVGL
reporting the wrong commit. Running this first turns that into a clear error.

## Building a release

```sh
export PATH=$(scripts/tools/toolchain/install_mips_toolchain.sh "$HOME/.cache/compas-toolchain")/bin:$PATH

scripts/release/build-release.sh \
    --rootfs /path/to/unpacked-rootfs \
    --kernel /path/to/xImage \
    --label  v1.4.1_Mutiboot \
    --out    r1.upt
```

The root filesystem is the one from the matching upstream release, with
`packaging/dual-boot/S20sonix` installed as `/etc/init.d/S20sonix` and the
`/sonix` boot marker handling described in `packaging/dual-boot/README.md`.

## About the version label

The label is stamped into `src/core/system/runtime/app_version.c` **in the build
tree only**. The repository keeps upstream's `"Development Build"` default on
purpose: `mutiboot` and `cn-enhanced` would otherwise both edit that one line and
conflict on every rebase.

Upstream's Makefile also accepts `RELEASE_LABEL=vX.Y.Z`, but it validates the
string against `vX.Y.Z` and rejects suffixes, which is why the release script
does the substitution itself.

When upstream moves, bump the label with it — the current one is `v1.4.1_…`.
