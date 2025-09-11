# CMakeLists.txt for protocol tests

# Options to control which tests to build
option(BUILD_RCV_BUFFER_TEST "Build pwar_rcv_buffer_test" OFF)
option(BUILD_ROUTER_TEST "Build pwar_router_test" OFF)
option(BUILD_RING_BUFFER_TEST "Build pwar_ring_buffer_test" ON)
option(BUILD_SEND_RECEIVE_CHAIN_TEST "Build pwar_send_receive_chain_test" OFF)

# Find Check testing framework (optional)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CHECK check)
endif()

# Find required libraries
find_library(MATH_LIB m)
find_package(Threads REQUIRED)

# Protocol sources - use absolute paths relative to project root
set(PROTOCOL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(PROTOCOL_SOURCES
    ${PROTOCOL_DIR}/pwar_router.c
    ${PROTOCOL_DIR}/pwar_rcv_buffer.c
    ${PROTOCOL_DIR}/pwar_ring_buffer.c
)

# Check if pwar_send_buffer.c exists (it's referenced in tests but may not exist yet)
if(EXISTS ${PROTOCOL_DIR}/pwar_send_buffer.c)
    list(APPEND PROTOCOL_SOURCES ${PROTOCOL_DIR}/pwar_send_buffer.c)
    set(HAS_SEND_BUFFER TRUE)
else()
    set(HAS_SEND_BUFFER FALSE)
    message(STATUS "pwar_send_buffer.c not found - some tests may not build correctly")
endif()

# Include directories
include_directories(${PROTOCOL_DIR})

# Common function to set up test targets
function(add_protocol_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # Link libraries
    target_link_libraries(${TEST_NAME} ${MATH_LIB} Threads::Threads)
    
    # Add Check support if available
    if(CHECK_FOUND)
        target_include_directories(${TEST_NAME} PRIVATE ${CHECK_INCLUDE_DIRS})
        target_link_libraries(${TEST_NAME} ${CHECK_LIBRARIES})
        target_compile_options(${TEST_NAME} PRIVATE ${CHECK_CFLAGS_OTHER})
        
        # Add test to CTest
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endif()
endfunction()

# Protocol test executables - only build if enabled
if(BUILD_RCV_BUFFER_TEST)
    add_protocol_test(pwar_rcv_buffer_test 
        "pwar_rcv_buffer_test.c;${PROTOCOL_SOURCES}")
    message(STATUS "Building pwar_rcv_buffer_test")
endif()

if(BUILD_RING_BUFFER_TEST)
    add_protocol_test(pwar_ring_buffer_test 
        "pwar_ring_buffer_test.c;${PROTOCOL_DIR}/pwar_ring_buffer.c")
    message(STATUS "Building pwar_ring_buffer_test")
endif()

if(BUILD_ROUTER_TEST)
    add_protocol_test(pwar_router_test 
        "pwar_router_test.c;${PROTOCOL_SOURCES}")
    message(STATUS "Building pwar_router_test")
endif()

if(BUILD_SEND_RECEIVE_CHAIN_TEST AND HAS_SEND_BUFFER)
    add_protocol_test(pwar_send_receive_chain_test 
        "pwar_send_receive_chain_test.c;${PROTOCOL_SOURCES}")
    message(STATUS "Building pwar_send_receive_chain_test")
elif(BUILD_SEND_RECEIVE_CHAIN_TEST AND NOT HAS_SEND_BUFFER)
    message(STATUS "Cannot build pwar_send_receive_chain_test - missing pwar_send_buffer.c")
endif()

# Enable testing
if(CHECK_FOUND)
    enable_testing()
    message(STATUS "Check testing framework found - building protocol tests with Check support")
    message(STATUS "Run tests with: ninja test (or ctest)")
else()
    message(STATUS "Check testing framework not found - building protocol tests without Check support")
endif()

# Summary of what's being built
message(STATUS "Protocol test build summary:")
message(STATUS "  RCV Buffer Test: ${BUILD_RCV_BUFFER_TEST}")
message(STATUS "  Ring Buffer Test: ${BUILD_RING_BUFFER_TEST}")
message(STATUS "  Router Test: ${BUILD_ROUTER_TEST}")
message(STATUS "  Send/Receive Chain Test: ${BUILD_SEND_RECEIVE_CHAIN_TEST}")
