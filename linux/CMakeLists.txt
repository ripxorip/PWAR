# CMakeLists.txt for Linux targets
cmake_minimum_required(VERSION 3.15)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

# Find Qt5 for GUI
find_package(Qt5 COMPONENTS Core Widgets Quick Qml QUIET)

# Math library
find_library(MATH_LIB m)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/protocol)

# Protocol sources
set(PROTOCOL_SOURCES
    ${CMAKE_SOURCE_DIR}/protocol/pwar_router.c
    ${CMAKE_SOURCE_DIR}/protocol/pwar_rcv_buffer.c
    ${CMAKE_SOURCE_DIR}/protocol/latency_manager.c
)

# Build shared library
add_library(pwar SHARED
    libpwar.c
    ${PROTOCOL_SOURCES}
)

target_include_directories(pwar PRIVATE
    ${PIPEWIRE_INCLUDE_DIRS}
)

target_link_libraries(pwar 
    ${PIPEWIRE_LIBRARIES}
    ${MATH_LIB}
)

target_compile_options(pwar PRIVATE ${PIPEWIRE_CFLAGS_OTHER})

# CLI executable
add_executable(pwar_cli
    pwar_cli.c
)

target_link_libraries(pwar_cli
    pwar
    ${PIPEWIRE_LIBRARIES}
    ${MATH_LIB}
)

target_compile_options(pwar_cli PRIVATE ${PIPEWIRE_CFLAGS_OTHER})

# GUI executable (only if Qt5 is found)
if(Qt5_FOUND)
    # Enable automoc for Qt
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    
    # Qt5 sources
    qt5_add_resources(QT_RESOURCES resources.qrc)
    
    add_executable(pwar_gui
        pwar_gui.cpp
        PwarController.cpp
        PwarController.h
        ${QT_RESOURCES}
    )
    
    target_link_libraries(pwar_gui
        pwar
        Qt5::Core
        Qt5::Widgets
        Qt5::Quick
        Qt5::Qml
        ${PIPEWIRE_LIBRARIES}
        ${MATH_LIB}
    )
    
    target_compile_options(pwar_gui PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
    
    message(STATUS "Qt5 found - building GUI application")
else()
    message(STATUS "Qt5 not found - skipping GUI application")
endif()

# Torture test executable
add_executable(pwar_torture
    torture.c
)

target_include_directories(pwar_torture PRIVATE
    ${PIPEWIRE_INCLUDE_DIRS}
)

target_link_libraries(pwar_torture
    ${PIPEWIRE_LIBRARIES}
    ${MATH_LIB}
)

target_compile_options(pwar_torture PRIVATE ${PIPEWIRE_CFLAGS_OTHER})

# Windows simulator executable
add_executable(windows_sim
    windows_sim.c
    ${PROTOCOL_SOURCES}
)

target_include_directories(windows_sim PRIVATE
    ${PIPEWIRE_INCLUDE_DIRS}
)

target_link_libraries(windows_sim
    ${PIPEWIRE_LIBRARIES}
    ${MATH_LIB}
)

target_compile_options(windows_sim PRIVATE ${PIPEWIRE_CFLAGS_OTHER})

# Integration test subdirectory
add_subdirectory(test)

# Install targets
install(TARGETS pwar pwar_cli DESTINATION bin)

if(Qt5_FOUND)
    install(TARGETS pwar_gui DESTINATION bin)
    install(FILES pwar_gui.qml DESTINATION share/pwar)
    # Install desktop file
    install(FILES pwar.desktop DESTINATION share/applications)
endif()

# Install icon with proper naming convention
install(FILES ${CMAKE_SOURCE_DIR}/media/pwar_icon_256x256.png 
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME pwar.png)

# Create a local install target for testing
add_custom_target(install-local
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${CMAKE_SOURCE_DIR}/install
    COMMENT "Installing to local directory for testing"
)
