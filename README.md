<p align="center">
  <img src="media/pwar_logo.png" alt="PWAR Logo" width="300">
</p>

# PWAR: PipeWire ASIO Relay

🎵 **PWAR (PipeWire ASIO Relay)** is a zero-drift, real-time audio bridge between Windows ASIO hosts and Linux PipeWire. It enables ultra-low-latency audio streaming across platforms, making it ideal for musicians, streamers, and audio professionals.

---

## 🚀 Quick Start

1. 📦 **Download the [Steinberg ASIO SDK](https://www.steinberg.net/en/company/developers.html)** and extract it to `<project-root>/third_party/asiosdk/`.
2. 🛠️ **Build the Windows ASIO driver:**
   ```powershell
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022"
   cmake --build . --config Release
   ```
   The DLL will be at `build/windows/asio/PWARASIO.dll`.
3. 📝 **Register the driver:**
   Open a terminal as Administrator and run:
   ```powershell
   regsvr32.exe <full-path-to>\PWARASIO.dll
   ```
4. ⚙️ **Configure the Windows client:**
   Create `%USERPROFILE%\pwarASIO.cfg` (e.g., `C:\Users\YourName\pwarASIO.cfg`) with:
   ```
   udp_send_ip=192.168.66.2
   ```
   Replace with your Linux server's IP.
5. 🐧 **Build the Linux binaries:**
   ```sh
   nix develop
   meson setup build
   ninja -C build
   ```
   The binaries will be in `linux/build/`.
6. 🔗 **Run the Linux relay:**
   ```sh
   ./linux/build/pwarPipeWire --ip 192.168.66.3
   ```
   Replace `192.168.66.3` with the Windows ASIO host's IP.

---

## ✨ Features
- ⚡ Real-time, zero-drift audio relay
- 🪟 ASIO driver for Windows
- 🐧 PipeWire client for Linux
- 🛠️ Simple configuration

---

## 🏗️ Building

### 🪟 Windows (CMake)

#### 📦 Fetching the ASIO SDK
1. Download the [Steinberg ASIO SDK](https://www.steinberg.net/en/company/developers.html).
2. Extract the contents so that the folder structure is:
   ```
   <project-root>/third_party/asiosdk/
   ├── ASIO SDK 2.3.pdf
   ├── changes.txt
   ├── readme.txt
   ├── asio/
   ├── common/
   ├── driver/
   └── host/
   ```

#### 🛠️ Building the ASIO Driver
3. Install [CMake](https://cmake.org/download/) and a supported compiler (e.g., Visual Studio).
4. Open a terminal and run:
   ```powershell
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022" # or your version
   cmake --build . --config Release
   ```
5. The ASIO driver DLL will be in `build/windows/asio/PWARASIO.dll`.

### 🐧 Linux (Nix + Meson)
1. Install [Nix](https://nixos.org/download.html).
2. Build using the provided flake and Meson build system:
   ```sh
   nix develop
   meson setup build
   ninja -C build
   ```
3. The binaries will be in `linux/build/` (e.g., `pwar`, `pwar_torture`, `windows_sim`).

#### 🧪 Protocol Unit Tests
To build protocol unit tests:
```sh
cd protocol/test
meson setup build
ninja -C build
```
Test binaries will be in `protocol/test/build/`.

---

## 🪟 Registering the ASIO Driver on Windows
1. Open a terminal as Administrator.
2. Run:
   ```powershell
   regsvr32.exe <full-path-to>\PWARASIO.dll
   ```
   Replace `<full-path-to>` with the actual path to your built DLL.

---

## ⚙️ Configuration

Edit the `pwarASIO.cfg` file (create one under `%USERPROFILE%\pwarASIO.cfg`, e.g., `C:\Users\YourName\pwarASIO.cfg`) to set the target Linux machine's IP address:

```
udp_send_ip=192.168.66.2
```
Replace with your actual Linux server's IP.

---

## 🐧 Running the Linux Binary

On your Linux machine, run:
```sh
./linux/build/pwarPipeWire --ip 192.168.66.3
```
Replace `192.168.66.3` with the IP address of the Windows ASIO host to stream to.

---

## 🛠️ Troubleshooting
- Ensure both machines are on the same network and firewall allows traffic.
- Use matching sample rates and buffer sizes for best results.
- Check logs for errors if audio does not stream.

---

## 🗺️ Roadmap

- **Merge PR for variable length frames**: Enables runtime adjustment of buffer size and latency.
- **Package binaries for major Linux distributions + Windows ASIO Binary**: Simplifies installation and usage.
- **Add a cross-platform GUI**: Improves usability and configuration.
- **Reduce jitter for VM setups**: Explore virtio-vsock or ivshmem for inter-VM shared memory, bypassing the network stack for lower latency and jitter.

---

## 🤝 Contributing

I have limited time to spend on this project, so contributions are very welcome! If you have improvements, bug fixes, or new features, please open a Pull Request (PR). I appreciate your help in making PWAR even better!

⭐ If you like this project, please consider giving it a star on GitHub to show your support!

---

## 📄 License
📝 GPL-3 — See [LICENSE](LICENSE)
