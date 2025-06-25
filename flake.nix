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
      protocol = builtins.path { path = ./protocol; };
      pwarPkg = pkgs.stdenv.mkDerivation {
        pname = "pwarPipeWire";
        version = "0.1.0";
        src = ./linux;
        buildInputs = [ pkgs.pipewire.dev pkgs.pkg-config ];
        patchPhase = ''
          rm -rf protocol
          mkdir protocol
          cp -r "${protocol}/." protocol
        '';
        buildPhase = "make";
        installPhase = ''
          mkdir -p $out/bin
          cp _out/pwarPipeWire $out/bin/
        '';
      };

    in
    {
      formatter = pkgs.nixpkgs-fmt;
      devShell = pkgs.mkShell {
        buildInputs = with pkgs; [
          gcc
          pkg-config
          pipewire.dev
        ];
      };
      packages.default = pwarPkg;
      apps.default = flake-utils.lib.mkApp {
        drv = pwarPkg;
      };
    });
}
