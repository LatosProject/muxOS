{
  description = "muxOS — a minimal 32-bit x86 hobby kernel";

  inputs.nixpkgs.url = "git+https://mirrors.tuna.tsinghua.edu.cn/git/nixpkgs.git?ref=nixos-unstable&shallow=1";

  outputs =
    { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;

      mkSystem = system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          cross = pkgs.pkgsCross.i686-embedded;
          # mlibc is built with a hosted i686-linux cross compiler, as its
          # porting guide recommends (bare-metal GCC's headers/types disagree
          # with mlibc).
          linuxCross = pkgs.pkgsCross.gnu32;

          # nixpkgs' GRUB is Linux-only. This native Darwin build provides the
          # same i686-elf-grub-mkrescue interface on Apple Silicon.
          grub = if pkgs.stdenv.hostPlatform.isDarwin then
            pkgs.callPackage ./nix/pkgs/grub { inherit cross; }
          else
            pkgs.grub2;

          buildTools = pkgs.callPackage ./nix/build-tools.nix {
            inherit cross grub;
          };

          mlibc = pkgs.callPackage ./nix/pkgs/mlibc {
            cross = linuxCross;
            mlibcPort = ./ports/mlibc/sysdeps/muxos;
            crossFile = ./ports/mlibc/muxos.cross-file;
          };

          hello = pkgs.callPackage ./nix/pkgs/hello {
            cross = linuxCross;
            inherit mlibc;
            src = ./ports/mlibc;
            helloLd = ./ports/mlibc/hello.ld;
          };

          toybox = pkgs.callPackage ./nix/pkgs/toybox {
            cross = linuxCross;
            inherit mlibc;
            helloLd = ./ports/mlibc/hello.ld;
          };

          mrsh = pkgs.callPackage ./nix/pkgs/mrsh {
            cross = linuxCross;
            inherit mlibc;
            helloLd = ./ports/mlibc/hello.ld;
          };

          neatvi = pkgs.callPackage ./nix/pkgs/neatvi {
            cross = linuxCross;
            inherit mlibc;
            helloLd = ./ports/mlibc/hello.ld;
          };

          sfm = pkgs.callPackage ./nix/pkgs/sfm {
            cross = linuxCross;
            inherit mlibc;
            helloLd = ./ports/mlibc/hello.ld;
          };

          muxos = pkgs.callPackage ./nix/pkgs/muxos {
            inherit buildTools;
            src = self;
            # One multicall toybox binary; the applet names become hardlinks
            # in /bin at boot (see kernel/fs/memfs.c).
            programs = [
              {
                name = "toybox";
                path = "${toybox}/bin/toybox";
              }
              {
                name = "sh";
                path = "${mrsh}/bin/sh";
              }
              {
                name = "vi";
                path = "${neatvi}/bin/vi";
              }
              {
                name = "sfm";
                path = "${sfm}/bin/sfm";
              }
            ];
            applets = toybox.toys;
          };

          runQemu = pkgs.writeShellScriptBin "muxos-run" ''
            # Expose a host directory to the guest as /root via virtio-9p.
            # Override with MUXOS_SHARE=/some/dir.
            share="''${MUXOS_SHARE:-$PWD}"
            exec ${pkgs.qemu}/bin/qemu-system-i386 \
              -m 512M \
              -cdrom ${muxos}/muxos.iso \
              -virtfs local,path="$share",mount_tag=host0,security_model=none \
              "$@"
          '';
        in
        {
          inherit pkgs linuxCross buildTools mlibc hello toybox mrsh neatvi sfm muxos runQemu;
        };

      perSystem = forAllSystems mkSystem;
    in
    {
      devShells = forAllSystems (system:
        let cfg = perSystem.${system}; in {
          default = cfg.pkgs.mkShell {
            packages = cfg.buildTools ++ [
              cfg.linuxCross.stdenv.cc
              cfg.linuxCross.binutils
              cfg.pkgs.qemu
              cfg.pkgs.meson
              cfg.pkgs.ninja
              cfg.pkgs.pkg-config
            ];
            shellHook = ''
              export QEMUFLAGS="''${QEMUFLAGS:--display ${if cfg.pkgs.stdenv.hostPlatform.isDarwin then "cocoa,zoom-to-fit=on" else "gtk"}}"
            '';
          };
        });

      packages = forAllSystems (system:
        let cfg = perSystem.${system}; in {
          default = cfg.muxos;
          mlibc = cfg.mlibc;
          hello = cfg.hello;
          toybox = cfg.toybox;
          mrsh = cfg.mrsh;
          neatvi = cfg.neatvi;
          sfm = cfg.sfm;
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${perSystem.${system}.runQemu}/bin/muxos-run";
        };
      });
    };
}
