/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <signal.h>

#include <iostream>
#include <thread>

#include "rtsp/rtsp_server.h"

using namespace lmshao::rtsp;

// 全局服务器实例，用于信号处理
std::shared_ptr<RTSPServer> g_server;

// 信号处理函数
void signalHandler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";

    // 停止服务器
    if (g_server) {
        g_server->Stop();
    }

    exit(signum);
}

int main(int argc, char *argv[])
{
    // 注册信号处理
    signal(SIGINT, signalHandler);

    // 获取RTSP服务器实例
    g_server = RTSPServer::GetInstance();

    // 初始化服务器，默认监听所有网络接口的554端口
    std::string ip = "0.0.0.0";
    uint16_t port = 554;

    // 如果提供了命令行参数，使用指定的IP和端口
    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    std::cout << "Initializing RTSP server on " << ip << ":" << port << std::endl;

    // 初始化服务器
    if (!g_server->Init(ip, port)) {
        std::cerr << "Failed to initialize RTSP server" << std::endl;
        return 1;
    }

    // 启动服务器
    if (!g_server->Start()) {
        std::cerr << "Failed to start RTSP server" << std::endl;
        return 1;
    }

    std::cout << "RTSP server started successfully" << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    // 主线程等待，实际服务在网络线程中运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}