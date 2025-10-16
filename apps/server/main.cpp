/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <lmnet/lmnet_logger.h>
#include <lmrtsp/lmrtsp_logger.h>
#include <signal.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "h264_file_reader.h"
#include "lmrtsp/i_rtp_packetizer.h"
#include "lmrtsp/media_stream.h"
#include "lmrtsp/media_stream_info.h"
#include "lmrtsp/rtsp_server.h"
#include "lmrtsp/rtsp_session.h"

using namespace lmshao::lmrtsp;

// Global server instance for signal handling
std::shared_ptr<RTSPServer> g_server;
std::atomic<bool> g_running{true};

// H.264 file reader
std::shared_ptr<H264FileReader> g_h264_reader;

// Signal handler function
void signalHandler(int signum)
{
    std::cout << "Received interrupt signal (" << signum << "), stopping server..." << std::endl;

    // Stop components
    g_running = false;
    if (g_server) {
        g_server->Stop();
    }
    if (g_h264_reader) {
        g_h264_reader->Close();
    }

    exit(signum);
}

void printUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [ip] [port] [video_file] [stream_path]" << std::endl;
    std::cout << "  ip:        Server IP address (default: 0.0.0.0)" << std::endl;
    std::cout << "  port:      Server port (default: 8554)" << std::endl;
    std::cout << "  video_file: Path to H.264 video file (optional)" << std::endl;
    std::cout << "  stream_path: Stream path (default: /live)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " 0.0.0.0 8554 /home/liming/work/Luca-30s-720p.h264 /live" << std::endl;
    std::cout << "  " << program_name << " 127.0.0.1 8554" << std::endl;
    std::cout << std::endl;
    std::cout << "Access URL: rtsp://[ip]:[port][stream_path]" << std::endl;
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    std::string ip = "0.0.0.0";
    uint16_t port = 8554;
    std::string video_file;
    std::string stream_path = "/live";

    // Expected usage: ./rtsp-server [ip] [port] [video_file] [stream_path]
    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }
    if (argc >= 4) {
        video_file = argv[3];
    }
    if (argc >= 5) {
        stream_path = argv[4];
        if (stream_path[0] != '/') {
            stream_path = "/" + stream_path;
        }
    }

    // Register signal handler
    signal(SIGINT, signalHandler);

    InitLmnetLogger(lmshao::lmcore::LogLevel::kDebug);
    InitLmrtspLogger(lmshao::lmcore::LogLevel::kDebug);

    // Get RTSP server instance
    g_server = RTSPServer::GetInstance();

    std::cout << "Initializing RTSP server, listening address: " << ip << ":" << port << std::endl;

    // Initialize server
    if (!g_server->Init(ip, port)) {
        std::cerr << "RTSP server initialization failed" << std::endl;
        return 1;
    }

    // Load video file if provided
    if (!video_file.empty()) {
        g_h264_reader = std::make_shared<H264FileReader>(video_file);
        if (!g_h264_reader->Open()) {
            std::cerr << "Failed to open video file: " << video_file << std::endl;
            return 1;
        }

        // Create media stream info
        auto stream_info = std::make_shared<MediaStreamInfo>();
        stream_info->stream_path = stream_path;
        stream_info->media_type = "video";
        stream_info->codec = "H264";
        stream_info->payload_type = 96;
        stream_info->clock_rate = 90000;

        // Get video parameters from file
        uint32_t width, height;
        if (g_h264_reader->GetResolution(width, height)) {
            stream_info->width = width;
            stream_info->height = height;
        } else {
            // Default values
            stream_info->width = 1280;
            stream_info->height = 720;
        }

        stream_info->frame_rate = g_h264_reader->GetFrameRate();

        // Set SPS and PPS from file
        stream_info->sps = g_h264_reader->GetSPS();
        stream_info->pps = g_h264_reader->GetPPS();

        // Register media stream with server
        if (!g_server->AddMediaStream(stream_path, stream_info)) {
            std::cerr << "Failed to register media stream: " << stream_path << std::endl;
            return 1;
        }

        std::cout << "Registered video stream: " << stream_path << std::endl;
        std::cout << "  File: " << video_file << std::endl;
        std::cout << "  Resolution: " << stream_info->width << "x" << stream_info->height << std::endl;
        std::cout << "  Frame rate: " << stream_info->frame_rate << " fps" << std::endl;
        std::cout << "  Duration: " << g_h264_reader->GetDuration() << " seconds" << std::endl;
        std::cout << std::endl;
        std::cout << "Client can connect with: rtsp://" << ip << ":" << port << stream_path << std::endl;
        std::cout << std::endl;
    } else {
        std::cout << "No video file provided. Running in test mode." << std::endl;
        std::cout << "Usage: " << argv[0] << " [ip] [port] [video_file] [stream_path]" << std::endl;
        std::cout << "Example: " << argv[0] << " 0.0.0.0 8554 /home/liming/work/Luca-30s-720p.h264 /live" << std::endl;
        std::cout << std::endl;
    }

    // Log using the logger registry directly
    auto &logger = lmshao::lmcore::LoggerRegistry::GetLogger<lmshao::lmrtsp::LmrtspModuleTag>();
    logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmshao::lmcore::LogLevel::kDebug, __FILE__, __LINE__,
                                                             __FUNCTION__, "RTSP server initialized successfully");

    // Start server
    if (!g_server->Start()) {
        std::cerr << "RTSP server startup failed" << std::endl;
        return 1;
    }

    logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmshao::lmcore::LogLevel::kDebug, __FILE__, __LINE__,
                                                             __FUNCTION__, "RTSP server started successfully");

    std::cout << "RTSP server is running, press Ctrl+C to stop server" << std::endl;

    // Main loop to push media data
    uint32_t timestamp = 0;
    uint32_t frame_interval_ms = 40; // 25fps default

    if (g_h264_reader) {
        frame_interval_ms = 1000 / g_h264_reader->GetFrameRate();
    }

    std::cout << "Starting media push loop with " << frame_interval_ms << "ms interval" << std::endl;

    while (g_running) {
        auto sessions = g_server->GetSessions();
        bool has_playing_clients = false;

        for (auto &session_pair : sessions) {
            auto session = session_pair.second;
            const auto &media_streams = session->GetMediaStreams();

            for (const auto &stream : media_streams) {
                if (stream->GetState() == StreamState::PLAYING) {
                    has_playing_clients = true;
                    auto rtp_stream = std::dynamic_pointer_cast<RTPStream>(stream);

                    if (rtp_stream) {
                        MediaFrame frame;

                        if (g_h264_reader) {
                            // Read real H.264 data from file
                            std::vector<uint8_t> frame_data;
                            if (g_h264_reader->GetNextFrame(frame_data)) {
                                frame.data = std::move(frame_data);
                                frame.timestamp = timestamp;
                                frame.marker = true; // Mark as complete frame
                                rtp_stream->PushFrame(std::move(frame));
                            }
                        } else {
                            // Test mode - send dummy data
                            frame.data.assign(1024, 0xAB);
                            frame.timestamp = timestamp;
                            frame.marker = false;
                            rtp_stream->PushFrame(std::move(frame));
                        }
                    }
                }
            }
        }

        // Only increment timestamp if we have playing clients
        if (has_playing_clients) {
            if (g_h264_reader) {
                timestamp += 90000 / g_h264_reader->GetFrameRate(); // 90kHz clock rate
            } else {
                timestamp += 3600; // Default for 90kHz clock rate, 40ms frame duration
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(frame_interval_ms));
    }

    // Cleanup
    std::cout << "Shutting down..." << std::endl;
    if (g_h264_reader) {
        g_h264_reader->Close();
    }
    if (g_server) {
        g_server->Stop();
    }

    return 0;
}