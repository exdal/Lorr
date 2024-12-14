{
  description = "Lorr";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
  let
    each_system = nixpkgs.lib.genAttrs [ "x86_64-linux" ];
    pkgs = nixpkgs.legacyPackages;
  in rec {
    packages = each_system (system: {
      default = pkgs.${system}.callPackage ./default.nix {};
    });

    devShells = each_system (system: {
      default = pkgs.${system}.callPackage ./shell.nix {};
    });
  };
}
