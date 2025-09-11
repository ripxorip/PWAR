# Top-level CMakeLists.txt for PWAR
cmake_minimum_required(VERSION 3.15)
project(PWAR LANGUAGES C CXX)

# Set C and C++ standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Platform-specific builds
if(WIN32)
    # Windows-specific targets
    add_subdirectory(windows/asio)
    add_subdirectory(windows/simulator)
else()
    # Linux-specific targets
    add_subdirectory(linux)
    
    # Protocol tests (Linux only - tests not needed on Windows)
    add_subdirectory(protocol/test)
endif()