{ pkgs, cross, grub }:
with pkgs; [
  cross.stdenv.cc
  cross.binutils
  nasm
  grub
  xorriso
  mtools
  gnumake
]
