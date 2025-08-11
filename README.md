# rtsp
A lightweight RTSP/RTP stack implementation in C++.

## Clone with submodules

To clone this repository and automatically download all dependencies (including the network library), use:

```bash
git clone --recursive https://github.com/lmshao/rtsp
```

If you already cloned without --recursive, run:

```bash
git submodule update --init --recursive
```

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Run

```bash
./rtsp-server
```
