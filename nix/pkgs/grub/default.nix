{
  stdenv,
  lib,
  fetchurl,
  cross,
  bison,
  flex,
  gawk,
  help2man,
  python3,
  texinfo,
  xz,
}:

stdenv.mkDerivation {
  pname = "i686-elf-grub";
  version = "2.12";

  src = fetchurl {
    url = "mirror://gnu/grub/grub-2.12.tar.xz";
    hash = "sha256-88lzkffE6qZ3p44JDH6X5txHsW9lXwRoPr03vvf+D6o=";
  };

  nativeBuildInputs = [
    bison
    flex
    gawk
    help2man
    python3
    texinfo
    cross.stdenv.cc
    cross.binutils
  ];

  buildInputs = [ xz ];

  preConfigure = ''
    touch grub-core/extra_deps.lst
    mkdir build
    cd build
  '';

  configureScript = "../configure";
  configureFlags = [
    "--disable-werror"
    "--disable-nls"
    "--target=i686-elf"
    "--with-platform=pc"
    "--program-prefix=i686-elf-"
  ];

  # GRUB's link-format probe is unreliable when its host compiler targets
  # Darwin and its target compiler targets ELF. GNU ld calls this format
  # elf_i386, exposed to GCC through -Wl,-melf_i386.
  grub_cv_target_cc_link_format = "-melf_i386";

  enableParallelBuilding = true;

  meta = {
    description = "GNU GRUB bootloader tools targeting i686-elf";
    homepage = "https://www.gnu.org/software/grub/";
    license = lib.licenses.gpl3Plus;
    platforms = lib.platforms.darwin;
  };
}
