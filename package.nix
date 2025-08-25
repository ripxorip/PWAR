{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  ninja,
  pkg-config,
  pipewire,
  qt5,
  withGui ? true,
}:

stdenv.mkDerivation rec {
  pname = "pwar";
  version = "0.1.0";
  
  src = fetchFromGitHub {
    owner = "ripxorip";
    repo = "PWAR";
    rev = "v${version}";  # FIXME Tag a release
    hash = "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
  ] ++ lib.optionals withGui [
    qt5.wrapQtAppsHook
    qt5.qttools
  ];

  buildInputs = [
    pipewire.dev
  ] ++ lib.optionals withGui [
    qt5.qtbase
    qt5.qtdeclarative
    qt5.qtquickcontrols2
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_SKIP_BUILD_RPATH=FALSE"
    "-DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE"
    "-DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE"
    "-DCMAKE_INSTALL_RPATH=${placeholder "out"}/lib"
  ];

  enableParallelBuilding = true;

  installPhase = ''
    runHook preInstall
    
    # Install binaries
    mkdir -p $out/bin
    cp linux/pwar_cli $out/bin/
    
    # Install shared library
    mkdir -p $out/lib
    cp linux/libpwar.so $out/lib/
    
    ${lib.optionalString withGui ''
      if [ -f linux/pwar_gui ]; then
        cp linux/pwar_gui $out/bin/
        
        # Install QML files and resources
        mkdir -p $out/share/pwar
        if [ -f linux/pwar_gui.qml ]; then
          cp linux/pwar_gui.qml $out/share/pwar/
        fi
        
        # Install desktop file
        mkdir -p $out/share/applications
        if [ -f linux/pwar.desktop ]; then
          cp linux/pwar.desktop $out/share/applications/
        fi
        
        # Install icon
        mkdir -p $out/share/icons/hicolor/256x256/apps
        if [ -f media/pwar_icon_256x256.png ]; then
          cp media/pwar_icon_256x256.png $out/share/icons/hicolor/256x256/apps/pwar.png
        fi
      fi
    ''}
    
    runHook postInstall
  '';

  meta = with lib; {
    description = "PipeWire ASIO Relay - Zero-drift, real-time audio bridge between Windows ASIO hosts and Linux PipeWire";
    longDescription = ''
      PWAR (PipeWire ASIO Relay) is a zero-drift, real-time audio bridge between 
      Windows ASIO hosts and Linux PipeWire. It enables ultra-low-latency audio 
      streaming across platforms, making it ideal for musicians, streamers, and 
      audio professionals.
      
      This package includes:
      - pwar_cli: Command-line PipeWire client
      - libpwar.so: Shared library for audio processing
      ${lib.optionalString withGui "- pwar_gui: Qt-based graphical interface"}
    '';
    homepage = "https://github.com/ripxorip/PWAR";
    license = licenses.mit; # Update with your actual license
    maintainers = with maintainers; [ /* your-maintainer-handle */ ];
    platforms = platforms.linux;
    mainProgram = if withGui then "pwar_gui" else "pwar_cli";
  };
}
