# Top-level CMakeLists.txt for PWAR
cmake_minimum_required(VERSION 3.15)
project(PWAR)

add_subdirectory(windows/asio)
add_subdirectory(windows/torture)
# add_subdirectory(linux) # Uncomment when ready for Linux build
