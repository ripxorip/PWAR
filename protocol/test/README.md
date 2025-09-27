# Protocol Tests

This directory contains unit tests for the PWAR protocol components.

## Building Tests

From the project root, use:

```bash
# Configure with Ninja (only ring buffer test enabled by default)
cmake -G Ninja -S . -B build

# Build all enabled tests
ninja -C build

# Run tests (if Check framework is available)
ninja -C build test
# or
cd build && ctest
```

## Test Control Options

You can control which tests are built by setting CMake options:

```bash
# Enable/disable specific tests
cmake -G Ninja -S . -B build \
    -DBUILD_RING_BUFFER_TEST=ON \
    -DBUILD_RCV_BUFFER_TEST=OFF \
    -DBUILD_ROUTER_TEST=OFF \
    -DBUILD_SEND_RECEIVE_CHAIN_TEST=OFF

# Build only the ring buffer test (default)
cmake -G Ninja -S . -B build

# Enable all tests
cmake -G Ninja -S . -B build \
    -DBUILD_RCV_BUFFER_TEST=ON \
    -DBUILD_ROUTER_TEST=ON \
    -DBUILD_RING_BUFFER_TEST=ON \
    -DBUILD_SEND_RECEIVE_CHAIN_TEST=ON
```

## Available Tests

- **pwar_ring_buffer_test**: Comprehensive unit test for the ring buffer implementation
  - Tests basic push/pop operations  
  - Tests overrun/underrun behavior (critical for finding bugs)
  - Tests edge cases like wrap-around, zero samples, channel mismatches
  - Tests thread safety and statistics
  
- **pwar_rcv_buffer_test**: Tests for the receive buffer
- **pwar_router_test**: Tests for the packet router
- **pwar_send_receive_chain_test**: End-to-end tests (requires pwar_send_buffer.c)

## Running Individual Tests

After building, you can run tests directly:

```bash
# Run ring buffer test
./build/protocol/test/pwar_ring_buffer_test

# Run with verbose output
./build/protocol/test/pwar_ring_buffer_test -v
```

## Dependencies

- **Check framework** (optional but recommended): Provides structured unit testing
  - Install on Ubuntu/Debian: `sudo apt install check libcheck-dev`
  - Tests will build without Check but with limited functionality

- **pthread**: Required for ring buffer tests (thread safety testing)
- **math library**: Required for floating-point operations
