# PWAR Windows Client Simulator

A standalone Windows application that replicates the PWAR ASIO driver's network mechanics for testing purposes.

## Overview

This simulator provides the same UDP networking, IOCP-based processing, and PWAR protocol handling as the actual `pwarASIO.cpp` driver, but as a standalone executable that doesn't require ASIO registration or a DAW.

## Features

- **Identical Network Stack**: Uses the same IOCP-based UDP listener as the ASIO driver
- **Real-time Priority**: Sets thread priority to TIME_CRITICAL and registers with MMCSS
- **PWAR Protocol**: Full support for PWAR packet processing and latency management
- **Configuration**: Supports config files (same format as ASIO driver) and command-line options
- **Statistics**: Shows packet processing rates and timing information

## Building

The simulator is built automatically as part of the Windows build:

```cmd
cd build
cmake --build . --target pwar_client_simulator
```

The executable will be created at: `build/windows/simulator/pwar_client_simulator.exe`

## Usage

### Basic Usage
```cmd
# Run with defaults (connects to 192.168.66.2:8321)
pwar_client_simulator.exe

# Connect to localhost
pwar_client_simulator.exe -s 127.0.0.1

# Use custom buffer size and enable verbose output
pwar_client_simulator.exe -b 256 -v
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-s, --server <ip>` | Server IP address | `192.168.66.2` |
| `-p, --port <port>` | Server port | `8321` |
| `-c, --client-port <port>` | Client listening port | `8321` |
| `-b, --buffer <size>` | Buffer size in samples | `512` |
| `-n, --channels <count>` | Number of channels | `2` |
| `-r, --rate <rate>` | Sample rate | `48000` |
| `-f, --config <file>` | Config file path | `%USERPROFILE%\pwarASIO.cfg` |
| `-v, --verbose` | Enable verbose output | `false` |
| `-h, --help` | Show help message | - |

### Configuration File

The simulator reads the same config file as the ASIO driver:
- Default location: `%USERPROFILE%\pwarASIO.cfg`
- Format: `key=value` pairs

Example config file:
```
udp_send_ip=192.168.1.100
```

## How It Works

1. **Initialization**: Sets up UDP sockets, PWAR router, and audio buffers
2. **IOCP Listener**: High-performance async UDP packet reception (same as ASIO driver)
3. **Packet Processing**: Processes incoming PWAR packets using the same router logic
4. **Audio Simulation**: Copies input to output (simulates audio processing)
5. **Response**: Sends processed audio back to server with proper sequencing and timing

## Use Cases

- **Protocol Testing**: Test PWAR protocol changes without a DAW
- **Performance Testing**: Measure latency and throughput
- **Development**: Quick iteration during PWAR development
- **Debugging**: Isolate network issues from ASIO driver complexity
- **CI/CD**: Automated testing in build pipelines

## Differences from ASIO Driver

- No ASIO callbacks (simulated with simple audio processing)
- No COM/DirectShow registration required
- Standalone executable (no DLL)
- Command-line interface instead of DAW integration
- Additional statistics and verbose output

## Example Session

```cmd
> pwar_client_simulator.exe -v -s 127.0.0.1
PWAR Windows Client Simulator - Standalone testing tool
Replicates ASIO driver network mechanics for testing

PWAR Windows Client Simulator Configuration:
  Server:        127.0.0.1:8321
  Client port:   8321
  Channels:      2
  Buffer size:   512 samples
  Sample rate:   48000 Hz
  Verbose:       enabled

UDP sender initialized, target: 127.0.0.1:8321
UDP receiver bound to port 8321
IOCP Thread priority set to TIME_CRITICAL.
IOCP MMCSS registration succeeded.
Started IOCP listener thread, waiting for packets...
Simulator started successfully. Press Ctrl+C to stop.
Waiting for audio packets from PWAR server...

Processed 1000 packets in 1 seconds (sent: 1000)
Processed 2000 packets in 2 seconds (sent: 2000)
^C
Received signal 2, shutting down...

Shutting down...
IOCP listener thread stopped

Final Statistics:
  Runtime:           2 seconds
  Packets processed: 2048
  Packets sent:      2048
  Average rate:      1024.00 packets/sec
  Sample position:   1048576 samples (21.85 seconds)

Shutdown complete
```
