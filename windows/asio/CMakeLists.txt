# CMake build for PWAR ASIO driver
cmake_minimum_required(VERSION 3.10)
project(PWARASIO LANGUAGES CXX)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files for the ASIO driver
set(PWARASIO_SOURCES
    pwarASIO.cpp
    pwarASIOLog.cpp
    ../../../protocol/pwar_router.c
    ../../../protocol/latency_manager.c
    ../../../third_party/asiosdk/common/combase.cpp
    ../../../third_party/asiosdk/common/dllentry.cpp
    ../../../third_party/asiosdk/common/register.cpp
)

add_library(PWARASIO SHARED ${PWARASIO_SOURCES})

# Set output name to PWARASIO.dll
set_target_properties(PWARASIO PROPERTIES OUTPUT_NAME "PWARASIO")

if (MSVC)
    # # Add the .def file for proper symbol exports
    set_target_properties(PWARASIO PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/pwarASIO.def")
else()
    set_target_properties(PWARASIO PROPERTIES
        PREFIX ""                       
        OUTPUT_NAME "pwarASIO"          
        WINDOWS_EXPORT_ALL_SYMBOLS FALSE
    )

    # explicit exports
    set_property(TARGET PWARASIO PROPERTY
        LINK_FLAGS "-Wl,--export-all-symbols -Wl,--add-stdcall-alias"
    )

    target_sources(PWARASIO PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pwarASIO.rc)

    target_link_libraries(PWARASIO PRIVATE ws2_32 avrt)
endif()

target_include_directories(PWARASIO PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/asiosdk/common
)

target_compile_definitions(PWARASIO PRIVATE WIN32 _WINDOWS _CRT_SECURE_NO_DEPRECATE)

target_link_libraries(PWARASIO PRIVATE odbc32 odbccp32 winmm)

