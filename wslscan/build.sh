#!/usr/bin/env sh
# Builds the static Linux helpers that WinDirStat embeds for scanning WSL distributions.
# Requires Zig (https://ziglang.org), used only as a C cross-compiler for musl targets.
# Set ZIG to override the compiler command, e.g. ZIG="python3 -m ziglang".
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out="$here/../windirstat/res/wsl"
zig=${ZIG:-zig}
mkdir -p "$out"
for arch in x86_64 aarch64; do
    $zig cc -target "$arch-linux-musl" -std=c11 -O2 -static -s -Wall -Wextra -Werror \
        -fno-asynchronous-unwind-tables -o "$out/wds-wsl-scan-$arch" "$here/wds-wsl-scan.c" -lpthread
    echo "built $out/wds-wsl-scan-$arch"
done
