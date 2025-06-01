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
5. 🐧 **Build the Linux binary:**
   ```sh
   nix develop
   make
   ```
   The binary will be at `linux/_out/pwarPipeWire`.
6. 🔗 **Run the Linux relay:**
   ```sh
   ./linux/pwarPipeWire --ip 192.168.66.3
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

### 🐧 Linux (Nix + Makefile)
1. Install [Nix](https://nixos.org/download.html).
2. Build using the provided flake in the linux directory:
   ```sh
   nix develop
   make
   ```
3. The binary will be in `linux/_out/pwarPipeWire`.

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
./linux/pwarPipeWire --ip 192.168.66.3
```
Replace `192.168.66.3` with the IP address of the Windows ASIO host to stream to.

---

## 🛠️ Troubleshooting
- Ensure both machines are on the same network and firewall allows traffic.
- Use matching sample rates and buffer sizes for best results.
- Check logs for errors if audio does not stream.

---

## 📄 License
📝 GPL-3 — See [LICENSE](LICENSE)
