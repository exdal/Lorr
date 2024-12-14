{ callPackage, stdenv,
  llvmPackages_19,
  cmake, ninja, gnumake, xmake,
  pkg-config, python313, fzf,
  zlib, xorg, glfw
}:
let
  main = callPackage ./default.nix {};
in
main.overrideAttrs rec {
  nativeBuildInputs = [
    cmake
    ninja
    gnumake
    xmake

    llvmPackages_19.clang-tools
    llvmPackages_19.clangUseLLVM
    llvmPackages_19.llvm
    llvmPackages_19.libcxx.dev

    pkg-config
    python313
    fzf

    zlib.dev
    xorg.libX11.dev
    xorg.libXi.dev
    xorg.libXrandr
    xorg.libXcursor
    xorg.libXinerama
    glfw
  ];
}
