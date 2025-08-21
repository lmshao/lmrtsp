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

#include "rtsp_log.h"
#include "rtsp_server.h"
#include "rtsp/media_stream.h"

using namespace lmshao::rtsp;

// Global server instance for signal handling
std::shared_ptr<RTSPServer> g_server;

// Signal handler function
void signalHandler(int signum)
{
    std::cout << "Received interrupt signal (" << signum << "), stopping server..." << std::endl;

    // Stop server
    if (g_server) {
        g_server->Stop();
    }

    exit(signum);
}

int main(int argc, char *argv[])
{
    // Register signal handler
    signal(SIGINT, signalHandler);

    // Get RTSP server instance
    g_server = RTSPServer::GetInstance();

    // Initialize server, default listening on all network interfaces port 8554
    std::string ip = "0.0.0.0";
    uint16_t port = 8554;

    // If command line arguments are provided, use specified IP and port
    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    std::cout << "Initializing RTSP server, listening address: " << ip << ":" << port << std::endl;

    // Initialize server
    if (!g_server->Init(ip, port)) {
        std::cerr << "RTSP server initialization failed" << std::endl;
        return 1;
    }

    RTSP_LOGD("RTSP server initialized successfully");

    // Start server
    if (!g_server->Start()) {
        std::cerr << "RTSP server startup failed" << std::endl;
        return 1;
    }

    RTSP_LOGD("RTSP server started successfully");
    std::cout << "RTSP server is running, press Ctrl+C to stop server" << std::endl;

    // Main loop to push media data
    uint32_t timestamp = 0;
    while (true) {
        auto sessions = g_server->GetSessions();
        for (auto &session_pair : sessions) {
            auto session = session_pair.second;
            const auto &media_streams = session->GetMediaStreams();
            for (const auto &stream : media_streams) {
                if (stream->GetState() == StreamState::PLAYING) {
                    auto rtp_stream = std::dynamic_pointer_cast<RTPStream>(stream);
                    if (rtp_stream) {
                        rtp::MediaFrame frame;
                        frame.data.assign(1024, 0xAB);
                        frame.timestamp = timestamp;
                        frame.marker = false;
                        rtp_stream->PushFrame(std::move(frame));
                    }
                }
            }
        }

        timestamp += 3600; // For 90kHz clock rate, 40ms frame duration
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    return 0;
}
