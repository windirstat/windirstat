# wds-wsl-scan

WinDirStat scans WSL 2 distributions by running this small Linux program inside the distribution.
It walks a directory tree with parallel `getdents64` and `statx` calls and streams one binary record
per entry to standard output (the layout is documented at the top of `wds-wsl-scan.c` and parsed by
`windirstat/FinderWsl.cpp`). This is much faster than listing the tree through the Plan 9 file server
behind `\\wsl.localhost`, which costs a round trip for every directory and file.

WinDirStat embeds the executables from `windirstat/res/wsl` as resources. To scan, it starts
`wsl.exe --distribution <name> --user root --exec /bin/sh -c <script>` and writes the helper for the
machine's processor to the script's standard input; the script copies it to a temporary file, runs
it, and removes it. The helper never enters other mounted filesystems (including bind mounts of the
same disk), so Windows drives under `/mnt` are not counted twice.

## Building

The helpers are static musl executables built with [Zig](https://ziglang.org/) 0.13.0 as a C
cross-compiler. The build is reproducible: CI rebuilds them and fails if the committed files differ.

```sh
python3 -m pip install ziglang==0.13.0.post1   # or install Zig 0.13.0 another way
ZIG="python3 -m ziglang" ./build.sh             # writes ../windirstat/res/wsl/wds-wsl-scan-{x86_64,aarch64}
```

## License

The helper is part of WinDirStat and is distributed under the GPL v3 or later. The executables are
statically linked with musl libc and Zig's compiler runtime, both under the MIT license; see
`THIRD-PARTY-NOTICES.txt`.
