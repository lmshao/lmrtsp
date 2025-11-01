/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 *
 * @brief RTSP Video On Demand (VOD) Server
 * Similar to live555MediaServer, this server automatically discovers
 * and serves media files from a specified directory.
 */

#include <lmnet/lmnet_logger.h>
#include <signal.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <thread>

#include "h264_file_reader.h"
#include "lmnet/lmnet_logger.h"
#include "lmrtsp/lmrtsp_logger.h"
#include "lmrtsp/media_stream_info.h"
#include "lmrtsp/rtsp_server.h"
#include "lmrtsp/rtsp_session.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmnet;
using namespace lmshao::lmcore;
namespace fs = std::filesystem;

// Global server components
std::shared_ptr<RtspServer> g_server;
std::atomic<bool> g_running{true};
std::string g_media_directory;

// Media file管理
struct MediaFile {
    std::string filename;
    std::string stream_path; // RTSP URL path
    std::string file_path;   // Full file path
    std::string codec;       // H264, AAC, MP2T
    std::shared_ptr<H264FileReader> reader;
};

std::map<std::string, MediaFile> g_media_files;
std::mutex g_media_mutex;

// Signal handler
void signalHandler(int signum)
{
    std::cout << "\nReceived interrupt signal (" << signum << "), stopping server..." << std::endl;
    g_running = false;

    if (g_server) {
        g_server->Stop();
    }

    exit(signum);
}

// Determine codec from file extension
std::string GetCodecFromExtension(const std::string &filename)
{
    std::string ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".h264" || ext == ".264") {
        return "H264";
    } else if (ext == ".aac") {
        return "AAC";
    } else if (ext == ".ts" || ext == ".m2ts") {
        return "MP2T";
    }

    return "";
}

// Scan media directory and register streams
bool ScanMediaDirectory(const std::string &directory)
{
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        std::cerr << "Error: Media directory does not exist or is not a directory: " << directory << std::endl;
        return false;
    }

    std::cout << "\n=== Scanning media directory: " << directory << " ===" << std::endl;

    int fileCount = 0;

    try {
        for (const auto &entry : fs::directory_iterator(directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string filename = entry.path().filename().string();
            std::string filepath = entry.path().string();
            std::string codec = GetCodecFromExtension(filename);

            if (codec.empty()) {
                continue; // Skip unsupported files
            }

            // Generate stream path from filename (keep full filename with extension)
            std::string streamPath = "/" + filename;

            // Create MediaFile entry
            MediaFile media;
            media.filename = filename;
            media.stream_path = streamPath;
            media.file_path = filepath;
            media.codec = codec;

            // For H.264 files, load parameters
            if (codec == "H264") {
                media.reader = std::make_shared<H264FileReader>(filepath);
                if (!media.reader->Open()) {
                    std::cerr << "Warning: Failed to open H.264 file: " << filepath << std::endl;
                    continue;
                }

                // Create and register media stream
                auto streamInfo = std::make_shared<MediaStreamInfo>();
                streamInfo->stream_path = streamPath;
                streamInfo->media_type = "video";
                streamInfo->codec = "H264";
                streamInfo->payload_type = 96;
                streamInfo->clock_rate = 90000;

                // Get video parameters
                uint32_t width, height;
                if (media.reader->GetResolution(width, height)) {
                    streamInfo->width = width;
                    streamInfo->height = height;
                } else {
                    streamInfo->width = 1920;
                    streamInfo->height = 1080;
                }

                streamInfo->frame_rate = media.reader->GetFrameRate();
                streamInfo->sps = media.reader->GetSPS();
                streamInfo->pps = media.reader->GetPPS();

                if (!g_server->AddMediaStream(streamPath, streamInfo)) {
                    std::cerr << "Warning: Failed to register stream: " << streamPath << std::endl;
                    media.reader->Close();
                    continue;
                }

                std::cout << "  [" << ++fileCount << "] " << filename << std::endl;
                std::cout << "      Stream:     rtsp://localhost:8554" << streamPath << std::endl;
                std::cout << "      Codec:      " << codec << std::endl;
                std::cout << "      Resolution: " << streamInfo->width << "x" << streamInfo->height << std::endl;
                std::cout << "      Frame rate: " << streamInfo->frame_rate << " fps" << std::endl;
                std::cout << "      Duration:   " << media.reader->GetDuration() << " seconds" << std::endl;
            }
            // Add support for other codecs here (AAC, TS)

            std::lock_guard<std::mutex> lock(g_media_mutex);
            g_media_files[streamPath] = media;
        }
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return false;
    }

    std::cout << "\n=== Found " << fileCount << " media file(s) ===" << std::endl;
    return fileCount > 0;
}

void printUsage(const char *programName)
{
    std::cout << "\nRTSP VOD Server - Video On Demand Service\n" << std::endl;
    std::cout << "Usage: " << programName << " [options] <media_directory>\n" << std::endl;

    std::cout << "Parameters:" << std::endl;
    std::cout << "  media_directory  Directory containing media files (.h264, .aac, .ts)" << std::endl;
    std::cout << "" << std::endl;

    std::cout << "Options:" << std::endl;
    std::cout << "  -ip <address>    Server IP (default: 0.0.0.0)" << std::endl;
    std::cout << "  -port <number>   Port number (default: 8554)" << std::endl;
    std::cout << "  -h, --help       Show this help message" << std::endl;
    std::cout << "" << std::endl;

    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " D:\\videos" << std::endl;
    std::cout << "  " << programName << " -ip 127.0.0.1 -port 8554 /home/user/videos" << std::endl;
    std::cout << "" << std::endl;

    std::cout << "Playback:" << std::endl;
    std::cout << "  The server will automatically discover all media files in the directory." << std::endl;
    std::cout << "  For file \"movie.h264\", use: rtsp://server:8554/movie.h264" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "  ffplay rtsp://localhost:8554/movie.h264" << std::endl;
    std::cout << "  vlc rtsp://localhost:8554/movie.h264" << std::endl;
    std::cout << "" << std::endl;

    std::cout << "Supported formats: .h264, .264, .aac, .ts, .m2ts" << std::endl;
}

int main(int argc, char *argv[])
{
    // Default parameters
    std::string ip = "0.0.0.0";
    uint16_t port = 8554;

    // Parse arguments
    if (argc < 2) {
        std::cerr << "Error: Missing media directory\n" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Check for help
    std::string firstArg = argv[1];
    if (firstArg == "-h" || firstArg == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    // Parse options
    int argIndex = 1;
    while (argIndex < argc) {
        std::string arg = argv[argIndex];

        if (arg == "-ip" && argIndex + 1 < argc) {
            ip = argv[++argIndex];
        } else if (arg == "-port" && argIndex + 1 < argc) {
            try {
                port = static_cast<uint16_t>(std::stoi(argv[++argIndex]));
            } catch (...) {
                std::cerr << "Error: Invalid port number" << std::endl;
                return 1;
            }
        } else if (arg[0] != '-') {
            // This is the media directory
            g_media_directory = arg;
            break;
        } else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }

        argIndex++;
    }

    if (g_media_directory.empty()) {
        std::cerr << "Error: Media directory not specified\n" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Register signal handler
    signal(SIGINT, signalHandler);

    // Initialize loggers
    InitLmnetLogger(LogLevel::kInfo);
    InitLmrtspLogger(LogLevel::kInfo);

    std::cout << "=== RTSP VOD Server ===" << std::endl;
    std::cout << "Listening on: " << ip << ":" << port << std::endl;
    std::cout << "Media directory: " << g_media_directory << std::endl;

    // Get server instance
    g_server = RtspServer::GetInstance();

    // Initialize server
    if (!g_server->Init(ip, port)) {
        std::cerr << "Failed to initialize RTSP server" << std::endl;
        return 1;
    }

    // Scan and register media files
    if (!ScanMediaDirectory(g_media_directory)) {
        std::cerr << "No media files found or failed to register streams" << std::endl;
        return 1;
    }

    // Start server
    if (!g_server->Start()) {
        std::cerr << "Failed to start RTSP server" << std::endl;
        return 1;
    }

    std::cout << "\n=== Server is running, press Ctrl+C to stop ===" << std::endl;

    // Main loop - push media data to playing clients
    while (g_running) {
        auto sessions = g_server->GetSessions();
        
        std::lock_guard<std::mutex> lock(g_media_mutex);
        
        for (auto &sessionPair : sessions) {
            auto session = sessionPair.second;
            
            if (!session->IsPlaying()) {
                continue;
            }
            
            // Get stream path for this session
            auto streamInfo = session->GetMediaStreamInfo();
            if (!streamInfo) {
                continue;
            }
            
            std::string streamPath = streamInfo->stream_path;
            
            auto it = g_media_files.find(streamPath);
            if (it == g_media_files.end()) {
                continue;
            }
            
            MediaFile &media = it->second;
            
            // Push frame based on codec
            if (media.codec == "H264" && media.reader) {
                std::vector<uint8_t> frameData;
                if (media.reader->GetNextFrame(frameData)) {
                    MediaFrame frame;
                    frame.data = DataBuffer::Create(frameData.size());
                    frame.data->Assign(frameData.data(), frameData.size());
                    frame.timestamp = 0; // Will be set by session
                    session->PushFrame(std::move(frame));
                } else {
                    // End of file, loop back
                    media.reader->Reset();
                }
            }
            // Add handlers for other codecs here
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(40)); // ~25fps
    }

    // Cleanup
    std::cout << "\nShutting down..." << std::endl;

    {
        std::lock_guard<std::mutex> lock(g_media_mutex);
        for (auto &pair : g_media_files) {
            if (pair.second.reader) {
                pair.second.reader->Close();
            }
        }
        g_media_files.clear();
    }

    if (g_server) {
        g_server->Stop();
    }

    std::cout << "Server stopped" << std::endl;
    return 0;
}
