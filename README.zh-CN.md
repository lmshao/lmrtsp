# Lmrtsp

一个现代C++ RTSP/RTP协议栈实现，具有高性能流媒体、全面的协议支持和跨平台兼容性。基于lmcore和lmnet库构建，为RTSP服务器/客户端应用程序、媒体流传输和实时传输协议处理提供完整解决方案。

## 功能特性

- **完整的RTSP实现**：全面支持RTSP 1.0协议标准方法（OPTIONS、DESCRIBE、SETUP、PLAY、PAUSE、TEARDOWN）
- **RTP/RTCP支持**：实时传输协议，支持H.264/AAC分包
- **高性能流媒体**：异步I/O与优化的媒体数据处理
- **会话管理**：高级连接和媒体会话生命周期管理
- **传输灵活性**：TCP/UDP传输模式，支持自动协商
- **MPEG-TS支持**：传输流复用/解复用功能
- **跨平台** 
- **生产就绪**：全面的单元测试、示例和应用程序

## 安装

### 前置依赖

Lmrtsp依赖于[lmcore库](https://github.com/lmshao/lmcore)和[lmnet库](https://github.com/lmshao/lmnet)。您有两种方式来满足这些依赖：

#### 方式1：系统安装（推荐）

首先将依赖安装到系统中：

```bash
# 克隆并安装lmcore
git clone https://github.com/lmshao/lmcore.git
cd lmcore
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install  # 安装到 /usr/local

# 克隆并安装lmnet
git clone https://github.com/lmshao/lmnet.git
cd lmnet
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install  # 安装到 /usr/local
```

然后构建lmrtsp：

```bash
git clone https://github.com/lmshao/lmrtsp.git
cd lmrtsp
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### 方式2：本地开发设置

将lmcore、lmnet和lmrtsp放在同一个父目录下：

```bash
# 目录结构应该是：
# parent_directory/
# ├── lmcore/     (在此克隆lmcore)
# ├── lmnet/      (在此克隆lmnet)
# └── lmrtsp/     (在此克隆lmrtsp)

git clone https://github.com/lmshao/lmcore.git
git clone https://github.com/lmshao/lmnet.git
git clone https://github.com/lmshao/lmrtsp.git

cd lmrtsp
mkdir build && cd build
cmake ..  # 将自动在兄弟目录中查找依赖
make -j$(nproc)
```

### 构建配置

CMake构建系统将自动：
1. 首先尝试查找系统安装的lmcore和lmnet（`find_package`）
2. 如果未找到，在兄弟目录`../lmcore`和`../lmnet`中查找
3. 如果都未找到，显示包含安装说明的有用错误消息

## 快速开始

创建一个简单的RTSP服务器：

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
    // 注册信号处理器
    signal(SIGINT, signalHandler);

    // 初始化日志
    InitLmrtspLogger(lmshao::lmcore::LogLevel::kDebug);
    
    // 获取RTSP服务器实例
    g_server = RTSPServer::GetInstance();

    // 初始化服务器
    std::string ip = "0.0.0.0";
    uint16_t port = 8554;
    if (!g_server->Init(ip, port)) {
        std::cerr << "RTSP服务器初始化失败" << std::endl;
        return 1;
    }

    // 启动服务器
    if (!g_server->Start()) {
        std::cerr << "RTSP服务器启动失败" << std::endl;
        return 1;
    }

    std::cout << "RTSP服务器运行在 " << ip << ":" << port << std::endl;
    std::cout << "按Ctrl+C停止服务器" << std::endl;

    // 保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

更多示例可以在[`examples/`](examples/)目录和[`apps/`](apps/)目录中找到。

## API参考

- 详细的API文档请参见[`include/lmrtsp/`](include/lmrtsp/)中的头文件
- 关键类：`RTSPServer`、`RtspServerSession`、`MediaStream`、`RTPSession`等

## 应用程序

项目包含了开箱即用的应用程序：

- **rtsp-server**：完整的RTSP流媒体服务器
- **rtsp-client**：用于测试和媒体消费的RTSP客户端

```bash
# 运行RTSP服务器
./build/bin/rtsp-server

# 运行RTSP客户端
./build/bin/rtsp-client rtsp://localhost:8554/stream
```

## 测试

构建后运行单元测试：

```bash
cd build
ctest --output-on-failure
```

## 示例

[`examples/`](examples/)目录包含：

- **rtsp_protocol_demo**：RTSP协议演示
- **ts_muxer**：MPEG-TS复用示例
- **ts_demuxer**：MPEG-TS解复用示例

## 贡献

欢迎贡献！请为错误报告、功能请求或改进提交issue或pull request。

## 许可证

本项目基于MIT许可证。详细信息请参见[LICENSE](LICENSE)。
