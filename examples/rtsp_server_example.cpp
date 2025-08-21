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

#include "rtsp/media_stream.h"
#include "rtsp/rtp/i_rtp_packetizer.h"
#include "rtsp/rtsp_server.h"

using namespace lmshao::rtsp;

// Global server instance for signal handling
std::shared_ptr<RTSPServer> g_server;

// Signal handler function
void signalHandler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";

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

    // Initialize server, default listening on all network interfaces port 554
    std::string ip = "0.0.0.0";
    uint16_t port = 8554;

    // If command line arguments are provided, use specified IP and port
    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    std::cout << "Initializing RTSP server on " << ip << ":" << port << std::endl;

    // Initialize server
    if (!g_server->Init(ip, port)) {
        std::cerr << "Failed to initialize RTSP server" << std::endl;
        return 1;
    }

    // Start server
    if (!g_server->Start()) {
        std::cerr << "Failed to start RTSP server" << std::endl;
        return 1;
    }

    std::cout << "RTSP server started successfully" << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    // Main thread waits, actual service runs in network thread
    while (true) {
        auto sessions = g_server->GetSessions();
        for (auto &session_pair : sessions) {
            auto session = session_pair.second;
            for (int i = 0; i < 2; i++) { // Assuming max 2 media streams
                auto media_stream = session->GetMediaStream(i);
                if (media_stream) {
                    auto rtp_stream = std::dynamic_pointer_cast<RTPStream>(media_stream);
                    if (rtp_stream && rtp_stream->GetState() == lmshao::rtsp::StreamState::PLAYING) {
                        rtp::MediaFrame frame;
                        frame.data.assign(1024, 'a'); // Dummy data
                        frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        rtp_stream->PushFrame(std::move(frame));
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    return 0;
}