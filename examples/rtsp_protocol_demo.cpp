/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <signal.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "lmrtsp/irtsp_server_callback.h"
#include "lmrtsp/media_stream_info.h"
#include "lmrtsp/rtsp_server.h"

using namespace lmshao::lmrtsp;

// Global server instance for signal handling
std::shared_ptr<RTSPServer> g_server;

// RTSP Server Callback Implementation
class RTSPProtocolCallback : public IRTSPServerCallback {
public:
    void OnClientConnected(const std::string &client_ip, const std::string &user_agent) override
    {
        std::cout << "[INFO] Client connected: " << client_ip << ", User-Agent: " << user_agent << std::endl;
    }

    void OnClientDisconnected(const std::string &client_ip) override
    {
        std::cout << "[INFO] Client disconnected: " << client_ip << std::endl;
    }

    void OnStreamRequested(const std::string &stream_path, const std::string &client_ip) override
    {
        std::cout << "[INFO] Stream requested: " << stream_path << " from " << client_ip << std::endl;
    }

    void OnSetupReceived(const std::string &client_ip, const std::string &transport,
                         const std::string &stream_path) override
    {
        std::cout << "[INFO] SETUP received from " << client_ip << " for " << stream_path
                  << ", Transport: " << transport << std::endl;
    }

    void OnPlayReceived(const std::string &client_ip, const std::string &stream_path, const std::string &range) override
    {
        std::cout << "[INFO] PLAY received from " << client_ip << " for " << stream_path;
        if (!range.empty()) {
            std::cout << ", Range: " << range;
        }
        std::cout << std::endl;
    }

    void OnPauseReceived(const std::string &client_ip, const std::string &stream_path) override
    {
        std::cout << "[INFO] PAUSE received from " << client_ip << " for " << stream_path << std::endl;
    }

    void OnTeardownReceived(const std::string &client_ip, const std::string &stream_path) override
    {
        std::cout << "[INFO] TEARDOWN received from " << client_ip << " for " << stream_path << std::endl;
    }

    bool OnAuthenticationRequired(const std::string &client_ip, const std::string &username,
                                  const std::string &password) override
    {
        std::cout << "[INFO] Authentication request from " << client_ip << ", Username: " << username
                  << ", Password: " << password << std::endl;

        // Simple authentication logic (for demo purposes)
        if (username == "admin" && password == "123456") {
            std::cout << "[INFO] Authentication successful for " << client_ip << std::endl;
            return true;
        }

        std::cout << "[WARN] Authentication failed for " << client_ip << std::endl;
        return false;
    }

    void OnError(const std::string &client_ip, int error_code, const std::string &error_message) override
    {
        std::cout << "[ERROR] Error from " << client_ip << " (Code: " << error_code << "): " << error_message
                  << std::endl;
    }
};

// Signal handler function
void signalHandler(int signum)
{
    std::cout << "\n[INFO] Interrupt signal (" << signum << ") received." << std::endl;

    // Stop server
    if (g_server) {
        std::cout << "[INFO] Stopping RTSP server..." << std::endl;
        g_server->Stop();
    }

    exit(signum);
}

// Create sample media stream info
std::shared_ptr<MediaStreamInfo> createSampleVideoStream()
{
    auto stream_info = std::make_shared<MediaStreamInfo>();

    // Basic information
    stream_info->stream_path = "/live/stream1";
    stream_info->media_type = "video";
    stream_info->codec = "H264";

    // Video parameters
    stream_info->width = 1920;
    stream_info->height = 1080;
    stream_info->frame_rate = 30;
    stream_info->bitrate = 2000000; // 2Mbps

    // Encoding parameters
    stream_info->profile_level = "42e01e";

    // RTP parameters
    stream_info->payload_type = 96;
    stream_info->clock_rate = 90000;
    stream_info->ssrc = 0x12345678;

    // Control parameters
    stream_info->enabled = true;
    stream_info->max_packet_size = 1400;

    return stream_info;
}

std::shared_ptr<MediaStreamInfo> createSampleAudioStream()
{
    auto stream_info = std::make_shared<MediaStreamInfo>();

    // Basic information
    stream_info->stream_path = "/live/audio1";
    stream_info->media_type = "audio";
    stream_info->codec = "PCMA";

    // Audio parameters
    stream_info->sample_rate = 8000;
    stream_info->channels = 1;
    stream_info->bits_per_sample = 16;

    // RTP parameters
    stream_info->payload_type = 8; // PCMA
    stream_info->clock_rate = 8000;
    stream_info->ssrc = 0x87654321;

    // Control parameters
    stream_info->enabled = true;
    stream_info->max_packet_size = 1400;

    return stream_info;
}

void printUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [ip] [port]" << std::endl;
    std::cout << "  ip   - Server IP address (default: 0.0.0.0)" << std::endl;
    std::cout << "  port - Server port (default: 8554)" << std::endl;
    std::cout << "\nExample:" << std::endl;
    std::cout << "  " << program_name << " 192.168.1.100 8554" << std::endl;
    std::cout << "\nAvailable streams:" << std::endl;
    std::cout << "  rtsp://server_ip:port/live/stream1 (H.264 Video)" << std::endl;
    std::cout << "  rtsp://server_ip:port/live/audio1  (PCMA Audio)" << std::endl;
}

int main(int argc, char *argv[])
{
    std::cout << "=== RTSP Protocol Demo Server ===" << std::endl;

    // Show usage if help is requested
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printUsage(argv[0]);
        return 0;
    }

    // Register signal handler
    signal(SIGINT, signalHandler);

    // Get RTSP server instance
    g_server = RTSPServer::GetInstance();

    // Configure server parameters
    std::string ip = "0.0.0.0";
    uint16_t port = 8554;

    // Parse command line arguments
    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    std::cout << "[INFO] Initializing RTSP server on " << ip << ":" << port << std::endl;

    // Initialize server
    if (!g_server->Init(ip, port)) {
        std::cerr << "[ERROR] Failed to initialize RTSP server" << std::endl;
        return 1;
    }

    // Create and set callback
    auto callback = std::make_shared<RTSPProtocolCallback>();
    g_server->SetCallback(callback);
    std::cout << "[INFO] RTSP callback handler registered" << std::endl;

    // Add sample media streams
    auto video_stream = createSampleVideoStream();
    if (g_server->AddMediaStream(video_stream->stream_path, video_stream)) {
        std::cout << "[INFO] Added video stream: " << video_stream->stream_path << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to add video stream" << std::endl;
        return -1;
    }

    auto audio_stream = createSampleAudioStream();
    if (g_server->AddMediaStream(audio_stream->stream_path, audio_stream)) {
        std::cout << "[INFO] Added audio stream: " << audio_stream->stream_path << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to add audio stream" << std::endl;
        return -1;
    }

    // Start server
    if (!g_server->Start()) {
        std::cerr << "[ERROR] Failed to start RTSP server" << std::endl;
        return -1;
    }

    std::cout << "[INFO] RTSP server started successfully" << std::endl;
    std::cout << "[INFO] Server is listening on rtsp://" << ip << ":" << port << "/" << std::endl;
    std::cout << "[INFO] Available streams:" << std::endl;
    std::cout << "  - rtsp://" << ip << ":" << port << "/live/stream1 (H.264 Video)" << std::endl;
    std::cout << "  - rtsp://" << ip << ":" << port << "/live/audio1  (PCMA Audio)" << std::endl;
    std::cout << "[INFO] Press Ctrl+C to stop the server" << std::endl;
    std::cout << "[INFO] Authentication: username=admin, password=123456" << std::endl;
    std::cout << "========================================" << std::endl;

    // Main server loop - monitor connections and protocol interactions
    while (g_server->IsRunning()) {
        // Get current sessions
        auto sessions = g_server->GetSessions();

        // Print session statistics every 10 seconds
        static auto last_stats_time = std::chrono::steady_clock::now();
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_stats_time);

        if (elapsed.count() >= 10) {
            std::cout << "[STATS] Active sessions: " << sessions.size()
                      << ", Connected clients: " << g_server->GetClientCount() << std::endl;

            // List available streams
            auto stream_paths = g_server->GetMediaStreamPaths();
            if (!stream_paths.empty()) {
                std::cout << "[STATS] Available streams: ";
                for (const auto &path : stream_paths) {
                    std::cout << path << " ";
                }
                std::cout << std::endl;
            }

            last_stats_time = current_time;
        }

        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[INFO] RTSP server stopped" << std::endl;
    return 0;
}