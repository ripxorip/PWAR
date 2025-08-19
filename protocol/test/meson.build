cc = meson.get_compiler('c')
math_dep = cc.find_library('m')

protocol_src = ['../pwar_router.c', '../pwar_send_buffer.c', '../pwar_rcv_buffer.c']
protocol_inc = include_directories('..')

executable('pwar_rcv_buffer_test', ['pwar_rcv_buffer_test.c'] + protocol_src, include_directories: protocol_inc, dependencies: math_dep, install: false)
executable('pwar_router_test', ['pwar_router_test.c'] + protocol_src, include_directories: protocol_inc, dependencies: math_dep, install: false)
executable('pwar_send_receive_chain_test', ['pwar_send_receive_chain_test.c'] + protocol_src, include_directories: protocol_inc, dependencies: math_dep, install: false)
