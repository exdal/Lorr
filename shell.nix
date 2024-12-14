{ callPackage,
  llvmPackages_19,
  cmake, ninja, gnumake, xmake,
  python313,
  glibc
}:

let main = callPackage ./default.nix {};
in main.overrideAttrs (oa: {
  nativeBuildInputs = [
    cmake
    ninja
    gnumake
    xmake

    python313

    glibc
    glibc.dev

    llvmPackages_19.stdenv
    llvmPackages_19.clang-unwrapped
    llvmPackages_19.llvm
    llvmPackages_19.lld
    llvmPackages_19.libcxx.dev
  ];
})
