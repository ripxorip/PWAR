project('PWAR', 'c', version: '1.0', default_options: ['buildtype=release'])

cc = meson.get_compiler('c')
pwar_inc = include_directories('../protocol')

# PipeWire dependency
pipewire_dep = dependency('libpipewire-0.3', required: true)
math_dep = cc.find_library('m')

subdir('test')

# Sources for main targets
srcs = ['pwarPipeWire.c', '../protocol/pwar_router.c', '../protocol/pwar_rcv_buffer.c']
executable('pwarPipeWire', srcs, include_directories: pwar_inc, dependencies: [pipewire_dep, math_dep], install: false)
executable('pwar_torture', 'torture.c', include_directories: pwar_inc, dependencies: [pipewire_dep, math_dep], install: false)
executable('windows_sim', ['windows_sim.c', '../protocol/pwar_router.c', '../protocol/pwar_rcv_buffer.c'], include_directories: pwar_inc, dependencies: [pipewire_dep, math_dep], install: false)
