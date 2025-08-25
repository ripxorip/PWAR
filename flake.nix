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
      
      # Main package (with GUI by default)
      pwar = pkgs.callPackage ./default.nix { };
      
      # CLI-only variant for minimal installs
      pwar-cli = pkgs.callPackage ./default.nix { withGui = false; };

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
          qt5.wrapQtAppsHook
          # Package building tools
          nfpm
          gnumake
        ];

        shellHook = ''
          echo "PWAR Development Environment"
          echo "Commands available:"
          echo "  nix build .#pwar       - Build full package"
          echo "  nix build .#pwar-cli   - Build CLI-only package"  
          echo "  nix run .#pwar         - Run GUI"
          echo "  nix run .#pwar-cli     - Run CLI"
          echo "  make -f Makefile.packages packages - Build DEB/RPM packages"
        '';
      };
      
      packages = {
        default = pwar;
        pwar = pwar;
        pwar-cli = pwar-cli;
      };
      
      apps = {
        default = flake-utils.lib.mkApp {
          drv = pwar;
          name = "pwar_gui";
        };
        pwar = flake-utils.lib.mkApp {
          drv = pwar;
          name = "pwar_gui";
        };
        pwar-cli = flake-utils.lib.mkApp {
          drv = pwar-cli;
          name = "pwar_cli";
        };
      };
    });
}
