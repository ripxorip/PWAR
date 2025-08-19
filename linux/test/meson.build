test_src = ['integration_test.c']

cc = meson.get_compiler('c')
pwar_inc = include_directories('../../protocol')
pipewire_dep = dependency('libpipewire-0.3', required: true)
math_dep = cc.find_library('m')

executable('integration_test', test_src, include_directories: pwar_inc, dependencies: [pipewire_dep, math_dep], install: false)
