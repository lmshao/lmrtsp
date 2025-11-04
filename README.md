# Lmrtsp

A modern C++ RTSP/RTP stack implementation featuring high-performance streaming, comprehensive protocol support, and cross-platform compatibility. Built on top of lmcore and lmnet libraries, it provides a complete solution for RTSP server/client applications, media streaming, and real-time transport protocol handling.

## Features

- **Complete RTSP implementation**: Full RTSP 1.0 protocol support with standard methods (OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN)
- **RTP/RTCP support**: Real-time transport protocol with H.264/AAC packetization
- **High-performance streaming**: Asynchronous I/O with optimized media data handling
- **Session management**: Advanced connection and media session lifecycle handling
- **Transport flexibility**: TCP/UDP transport modes with automatic negotiation
- **MPEG-TS support**: Transport stream muxing/demuxing capabilities
- **Cross-platform**: Linux and Windows support with platform-optimized networking
- **Production-ready**: Comprehensive unit tests, examples, and applications

## Installation

### Prerequisites

Lmrtsp depends on the [lmcore library](https://github.com/lmshao/lmcore) and [lmnet library](https://github.com/lmshao/lmnet). You have two options to satisfy these dependencies:

#### Option 1: System Installation (Recommended)

Install dependencies to your system first:

```bash
# Clone and install lmcore
git clone https://github.com/lmshao/lmcore.git
cd lmcore
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install  # Install to /usr/local

# Clone and install lmnet
git clone https://github.com/lmshao/lmnet.git
cd lmnet
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install  # Install to /usr/local
```

Then build lmrtsp:

```bash
git clone https://github.com/lmshao/lmrtsp.git
cd lmrtsp
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### Option 2: Local Development Setup

Place lmcore, lmnet, and lmrtsp in the same parent directory:

```bash
# Directory structure should be:
# parent_directory/
# ├── lmcore/     (clone lmcore here)
# ├── lmnet/      (clone lmnet here)
# └── lmrtsp/     (clone lmrtsp here)

git clone https://github.com/lmshao/lmcore.git
git clone https://github.com/lmshao/lmnet.git
git clone https://github.com/lmshao/lmrtsp.git

cd lmrtsp
mkdir build && cd build
cmake ..  # Will automatically find dependencies in sibling directories
make -j$(nproc)
```

### Build Configuration

The CMake build system will automatically:
1. First try to find system-installed lmcore and lmnet (`find_package`)
2. If not found, look for them in sibling directories `../lmcore` and `../lmnet`
3. If neither is found, display helpful error messages with installation instructions

## Quick Start

Create a simple RTSP server:

```cpp
#include <lmrtsp/rtsp_server.h>
#include <lmrtsp/lmrtsp_logger.h>

#include <iostream>
#include <signal.h>

using namespace lmshao::lmrtsp;

RTSPServer* g_server = nullptr;

void signalHandler(int signal)
{
    if (g_server) {
        g_server->Stop();
    }
    exit(0);
}

int main()
{
    // Register signal handler
    signal(SIGINT, signalHandler);

    // Initialize logger
    InitLmrtspLogger(lmshao::lmcore::LogLevel::kDebug);
    
    // Get RTSP server instance
    g_server = RTSPServer::GetInstance();

    // Initialize server
    std::string ip = "0.0.0.0";
    uint16_t port = 8554;
    if (!g_server->Init(ip, port)) {
        std::cerr << "RTSP server initialization failed" << std::endl;
        return 1;
    }

    // Start server
    if (!g_server->Start()) {
        std::cerr << "RTSP server startup failed" << std::endl;
        return 1;
    }

    std::cout << "RTSP server is running on " << ip << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop server" << std::endl;

    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

More examples can be found in the [`examples/`](examples/) directory and [`apps/`](apps/) directory.

## API Reference

- See header files in [`include/lmrtsp/`](include/lmrtsp/) for detailed API documentation
- Key classes: `RTSPServer`, `RtspServerSession`, `MediaStream`, `RTPSession`, etc.

## Applications

The project includes ready-to-use applications:

- **rtsp-server**: Complete RTSP streaming server
- **rtsp-client**: RTSP client for testing and media consumption

```bash
# Run RTSP server
./build/bin/rtsp-server

# Run RTSP client  
./build/bin/rtsp-client rtsp://localhost:8554/stream
```

## Testing

Run unit tests after building:

```bash
cd build
ctest --output-on-failure
```

## Examples

The [`examples/`](examples/) directory contains:

- **rtsp_protocol_demo**: RTSP protocol demonstration
- **ts_muxer**: MPEG-TS muxing example
- **ts_demuxer**: MPEG-TS demuxing example

## Contributing

Contributions are welcome! Please open issues or pull requests for bug reports, feature requests, or improvements.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
