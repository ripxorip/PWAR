{ pkgs ? import <nixpkgs> {} }:
pkgs.stdenv.mkDerivation {
  pname = "pwarPipeWire";
  version = "0.1.0";
  src = ./.;
  buildInputs = [ pkgs.pipewire.dev pkgs.pkg-config ];
  buildPhase = "make -C linux";
  installPhase = ''
    mkdir -p $out/bin
    cp linux/_out/pwarPipeWire $out/bin/
  '';
}
