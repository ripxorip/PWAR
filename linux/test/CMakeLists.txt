# CMakeLists.txt for Linux integration tests
cmake_minimum_required(VERSION 3.15)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

# Math library
find_library(MATH_LIB m)

# Integration test executable
add_executable(integration_test
    integration_test.c
)

target_include_directories(integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/protocol
    ${PIPEWIRE_INCLUDE_DIRS}
)

target_link_libraries(integration_test
    ${PIPEWIRE_LIBRARIES}
    ${MATH_LIB}
)

target_compile_options(integration_test PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
