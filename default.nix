{ llvmPackages_19 }:
llvmPackages_19.stdenv.mkDerivation rec {
  name = "NixOS clang";
  version = "1.0.0";
  src = ./.;
}
