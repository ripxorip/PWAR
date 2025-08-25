{
  description = "Flake for building pwar for Linux";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/release-24.11";
    flake-utils.url = "github:numtide/flake-utils";

    nix-formatter-pack.url = "github:Gerschtli/nix-formatter-pack";
    nix-formatter-pack.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs =
    { self
    , nixpkgs
    , nix-formatter-pack
    , flake-utils
    }:

    flake-utils.lib.eachDefaultSystem (system:

    let

      pkgs = import nixpkgs {
        inherit system;
        overlays = [ ];
      };
      pwarPkg = import ./default.nix { inherit pkgs; };

    in
    {
      formatter = pkgs.nixpkgs-fmt;
      devShell = pkgs.mkShell {
        buildInputs = with pkgs; [
          gcc
          cmake
          ninja
          check
          pkg-config
          pipewire.dev
          qt5.full
        ];
      };
      packages.default = pwarPkg;
      apps.default = flake-utils.lib.mkApp {
        drv = pwarPkg;
      };
    });
}
