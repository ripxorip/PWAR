# CMakeLists.txt for Linux targets - Audio Backend Agnostic
cmake_minimum_required(VERSION 3.15)

# Math library (always required)
find_library(MATH_LIB m)

# Find required packages with optional fallbacks
find_package(PkgConfig REQUIRED)

# Try to find PipeWire (optional)
pkg_check_modules(PIPEWIRE libpipewire-0.3)
if(PIPEWIRE_FOUND)
    message(STATUS "PipeWire found - PipeWire backend will be available")
    set(HAVE_PIPEWIRE TRUE)
else()
    message(STATUS "PipeWire not found - PipeWire backend will be disabled")
    set(HAVE_PIPEWIRE FALSE)
endif()

# Try to find ALSA (optional)
pkg_check_modules(ALSA alsa)
if(ALSA_FOUND)
    message(STATUS "ALSA found - ALSA backend will be available")
    set(HAVE_ALSA TRUE)
else()
    message(STATUS "ALSA not found - ALSA backend will be disabled")
    set(HAVE_ALSA FALSE)
endif()

# Ensure at least one audio backend is available
if(NOT HAVE_PIPEWIRE AND NOT HAVE_ALSA)
    message(FATAL_ERROR "Neither PipeWire nor ALSA found. At least one audio backend is required.")
endif()

# Find Qt5 for GUI (optional)
find_package(Qt5 COMPONENTS Core Widgets Quick Qml QUIET)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/protocol)

# Protocol sources (backend agnostic)
set(PROTOCOL_SOURCES
    ${CMAKE_SOURCE_DIR}/protocol/pwar_ring_buffer.c
    ${CMAKE_SOURCE_DIR}/protocol/latency_manager.c
)

# Audio backend sources (conditional compilation)
set(AUDIO_BACKEND_SOURCES
    audio_backend.c
    simulated_backend.c     # Always available for testing
)

if(HAVE_ALSA)
    list(APPEND AUDIO_BACKEND_SOURCES alsa_backend.c)
endif()

if(HAVE_PIPEWIRE)
    list(APPEND AUDIO_BACKEND_SOURCES pipewire_backend.c)
endif()

# Build shared library with agnostic audio backend support
add_library(pwar SHARED
    libpwar.c
    ${PROTOCOL_SOURCES}
    ${AUDIO_BACKEND_SOURCES}
)

# Set compile definitions based on available backends
target_compile_definitions(pwar PRIVATE HAVE_SIMULATED=1)  # Always available

if(HAVE_ALSA)
    target_compile_definitions(pwar PRIVATE HAVE_ALSA=1)
    target_include_directories(pwar PRIVATE ${ALSA_INCLUDE_DIRS})
endif()

if(HAVE_PIPEWIRE)
    target_compile_definitions(pwar PRIVATE HAVE_PIPEWIRE=1)
    target_include_directories(pwar PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
endif()

# Link libraries conditionally
target_link_libraries(pwar ${MATH_LIB} pthread)

if(HAVE_ALSA)
    target_link_libraries(pwar ${ALSA_LIBRARIES})
    target_compile_options(pwar PRIVATE ${ALSA_CFLAGS_OTHER})
endif()

if(HAVE_PIPEWIRE)
    target_link_libraries(pwar ${PIPEWIRE_LIBRARIES})
    target_compile_options(pwar PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
endif()

# CLI executable (agnostic)
add_executable(pwar_cli
    pwar_cli.c
)

target_link_libraries(pwar_cli pwar)

# Set compile definitions for CLI based on available backends
if(HAVE_ALSA)
    target_compile_definitions(pwar_cli PRIVATE HAVE_ALSA=1)
endif()

if(HAVE_PIPEWIRE)
    target_compile_definitions(pwar_cli PRIVATE HAVE_PIPEWIRE=1)
endif()

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
    )
    
    # Set compile definitions for GUI based on available backends
    if(HAVE_ALSA)
        target_compile_definitions(pwar_gui PRIVATE HAVE_ALSA=1)
    endif()

    if(HAVE_PIPEWIRE)
        target_compile_definitions(pwar_gui PRIVATE HAVE_PIPEWIRE=1)
    endif()
    
    message(STATUS "Qt5 found - building GUI application")
else()
    message(STATUS "Qt5 not found - skipping GUI application")
endif()

# Client simulator executable (agnostic)
add_executable(client_simulator
    client_simulator.c
    ${PROTOCOL_SOURCES}
)

target_link_libraries(client_simulator pwar)

# Integration test subdirectory (only if PipeWire is available)
if(HAVE_PIPEWIRE)
    add_subdirectory(test)
else()
    message(STATUS "PipeWire not found - skipping integration tests")
endif()

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

# Print build configuration summary
message(STATUS "=== PWAR Build Configuration ===")
message(STATUS "Audio Backends:")
if(HAVE_ALSA)
    message(STATUS "  ALSA: ENABLED")
else()
    message(STATUS "  ALSA: DISABLED (not found)")
endif()

if(HAVE_PIPEWIRE)
    message(STATUS "  PipeWire: ENABLED")
else()
    message(STATUS "  PipeWire: DISABLED (not found)")
endif()

message(STATUS "  Simulated: ENABLED (always available for testing)")

message(STATUS "Executables:")
message(STATUS "  pwar_cli: YES (agnostic)")
if(Qt5_FOUND)
    message(STATUS "  pwar_gui: YES (agnostic)")
else()
    message(STATUS "  pwar_gui: NO (Qt5 not found)")
endif()
message(STATUS "  client_simulator: YES (agnostic)")
message(STATUS "================================")
