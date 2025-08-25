# CMakeLists.txt for protocol tests
cmake_minimum_required(VERSION 3.15)

# Find Check testing framework (optional)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CHECK check)
endif()

# Math library
find_library(MATH_LIB m)

# Protocol sources
set(PROTOCOL_SOURCES
    ../pwar_router.c
    ../pwar_rcv_buffer.c
)

# Check if pwar_send_buffer.c exists (it's referenced in tests but may not exist yet)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../pwar_send_buffer.c)
    list(APPEND PROTOCOL_SOURCES ../pwar_send_buffer.c)
    set(HAS_SEND_BUFFER TRUE)
else()
    set(HAS_SEND_BUFFER FALSE)
    message(WARNING "pwar_send_buffer.c not found - some tests may not build correctly")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Protocol test executables
add_executable(pwar_rcv_buffer_test
    pwar_rcv_buffer_test.c
    ${PROTOCOL_SOURCES}
)

target_link_libraries(pwar_rcv_buffer_test ${MATH_LIB})

if(CHECK_FOUND)
    target_include_directories(pwar_rcv_buffer_test PRIVATE ${CHECK_INCLUDE_DIRS})
    target_link_libraries(pwar_rcv_buffer_test ${CHECK_LIBRARIES})
    target_compile_options(pwar_rcv_buffer_test PRIVATE ${CHECK_CFLAGS_OTHER})
endif()

add_executable(pwar_router_test
    pwar_router_test.c
    ${PROTOCOL_SOURCES}
)

target_link_libraries(pwar_router_test ${MATH_LIB})

if(CHECK_FOUND)
    target_include_directories(pwar_router_test PRIVATE ${CHECK_INCLUDE_DIRS})
    target_link_libraries(pwar_router_test ${CHECK_LIBRARIES})
    target_compile_options(pwar_router_test PRIVATE ${CHECK_CFLAGS_OTHER})
endif()

add_executable(pwar_send_receive_chain_test
    pwar_send_receive_chain_test.c
    ${PROTOCOL_SOURCES}
)

target_link_libraries(pwar_send_receive_chain_test ${MATH_LIB})

if(CHECK_FOUND)
    target_include_directories(pwar_send_receive_chain_test PRIVATE ${CHECK_INCLUDE_DIRS})
    target_link_libraries(pwar_send_receive_chain_test ${CHECK_LIBRARIES})
    target_compile_options(pwar_send_receive_chain_test PRIVATE ${CHECK_CFLAGS_OTHER})
endif()

# Only build send receive chain test if send buffer exists
if(NOT HAS_SEND_BUFFER)
    set_target_properties(pwar_send_receive_chain_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
    message(STATUS "Excluding pwar_send_receive_chain_test from build due to missing pwar_send_buffer.c")
endif()

if(CHECK_FOUND)
    message(STATUS "Check testing framework found - building protocol tests with Check support")
else()
    message(STATUS "Check testing framework not found - building protocol tests without Check support")
endif()
