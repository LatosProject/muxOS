# muxOS

muxOS is a minimal 32-bit x86 hobby operating system. The Nix flake supports
building and running it from both x86_64 Linux and Apple Silicon macOS hosts.
On an M1 Mac the build is native, while the kernel and userspace are
cross-compiled for i686 and run in QEMU.

中文开发文档见 [`docs/DEVELOPMENT.zh-CN.md`](docs/DEVELOPMENT.zh-CN.md)。

## Apple Silicon (M1/M2/M3/M4)

Enable flakes for each command if they are not enabled globally:

```sh
nix --extra-experimental-features 'nix-command flakes' build
```

The ISO is available at `result/muxos.iso`. Run it with:

```sh
nix --extra-experimental-features 'nix-command flakes' run
```

Additional QEMU arguments can be appended after `--`. To share a different
host directory as `/root` inside muxOS, set `MUXOS_SHARE`:

```sh
MUXOS_SHARE="$HOME" nix --extra-experimental-features 'nix-command flakes' run
```

For an interactive development environment:

```sh
nix --extra-experimental-features 'nix-command flakes' develop
make
make run
```
