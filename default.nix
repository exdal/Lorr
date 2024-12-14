{ llvmPackages_19, stdenv }:
llvmPackages_19.stdenv.mkDerivation rec {
  name = "Lorr";
  version = "1.0.0";
  src = ./.;
}
