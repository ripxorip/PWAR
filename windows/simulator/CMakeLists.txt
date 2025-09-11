# CMakeLists.txt for PWAR Windows Client Simulator
cmake_minimum_required(VERSION 3.16)

# Create the simulator executable
add_executable(pwar_client_simulator
    client_simulator.cpp
    ${CMAKE_SOURCE_DIR}/protocol/latency_manager.c
)

# Include directories
target_include_directories(pwar_client_simulator PRIVATE
    ${CMAKE_SOURCE_DIR}/protocol
    ${CMAKE_SOURCE_DIR}/windows/asio
)

# Link libraries
target_link_libraries(pwar_client_simulator
    ws2_32
    avrt
)

# Set properties
set_target_properties(pwar_client_simulator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add compile definitions
target_compile_definitions(pwar_client_simulator PRIVATE
    _WIN32_WINNT=0x0601
)
