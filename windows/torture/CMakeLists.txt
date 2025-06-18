# CMakeLists.txt for torture test
add_executable(pwar_torture
    torture_main.cpp
)
target_include_directories(pwar_torture PRIVATE
    ${CMAKE_SOURCE_DIR}/protocol
)
