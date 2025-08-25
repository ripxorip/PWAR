project('PWAR', ['c', 'cpp'], version: '1.0', default_options: ['buildtype=release'])

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
pwar_inc = include_directories('../protocol')

# PipeWire dependency
pipewire_dep = dependency('libpipewire-0.3', required: true)
math_dep = cc.find_library('m')

subdir('test')

# Sources for main targets
srcs = ['pwarPipeWire.c', '../protocol/pwar_router.c', '../protocol/pwar_rcv_buffer.c', '../protocol/latency_manager.c']
executable('pwar_torture', 'torture.c', include_directories: pwar_inc, dependencies: [pipewire_dep, math_dep], install: false)
executable('windows_sim', ['windows_sim.c', '../protocol/pwar_router.c', '../protocol/pwar_rcv_buffer.c', '../protocol/latency_manager.c'], include_directories: pwar_inc, dependencies: [pipewire_dep, math_dep], install: false)

libpwar = library(
  'pwar',
  ['libpwar.c', '../protocol/pwar_router.c', '../protocol/pwar_rcv_buffer.c', '../protocol/latency_manager.c'],
  include_directories: [include_directories('.'), include_directories('../protocol')],
  dependencies: [pipewire_dep, math_dep],
  install: true,
)

executable(
  'pwar_cli',
  'pwar_cli.c',
  link_with: libpwar,
  dependencies: [pipewire_dep, math_dep],
  install: true,
)

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Widgets', 'Quick', 'Qml'], required: true)

moc_gen = qt5.preprocess(
  moc_headers: ['PwarController.h'],
  qresources: ['resources.qrc']
)

executable(
  'pwar_gui',
  ['pwar_gui.cpp', 'PwarController.cpp', moc_gen],
  include_directories: [include_directories('.'), include_directories('../protocol')],
  link_with: libpwar,
  dependencies: [qt5_dep, pipewire_dep, math_dep],
  install: true,
)

# Optionally install QML file
install_data('pwar_gui.qml')
install_data('../media/pwar_icon.png', install_dir: 'share/icons/hicolor/256x256/apps')