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

#include <lmrtsp/lmrtsp_logger.h>
#include <lmrtsp/media_stream_info.h>
#include <lmrtsp/rtsp_server.h>
#include <lmrtsp/rtsp_session.h>
#include <signal.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <thread>

#include "file_manager.h"
#include "h264_file_reader.h"
#include "session_manager.h"
#include "session_worker_thread.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmnet;
using namespace lmshao::lmcore;

// Media file manager
struct MediaFile {
    std::string filename;
    std::string stream_path; // RTSP URL path
    std::string file_path;   // Full file path
    std::string codec;       // H264, AAC, MP2T
    // Note: No longer storing H264FileReader, using MappedFile through FileManager
};

// Global server components
std::shared_ptr<RtspServer> g_server;
std::atomic<bool> g_running{true};
std::string g_media_directory;
std::map<std::string, MediaFile> g_media_files;
std::mutex g_media_mutex;

// Session event callback for managing worker threads
class SessionEventCallback : public lmshao::lmrtsp::IRtspServerCallback {
public:
    void OnSessionCreated(std::shared_ptr<RtspSession> session) override
    {
        std::cout << "Session created: " << session->GetSessionId() << std::endl;
    }

    void OnSessionDestroyed(const std::string &session_id) override
    {
        std::cout << "Session destroyed: " << session_id << std::endl;
        // Stop the worker thread for this session
        SessionManager::GetInstance().StopSession(session_id);
    }

    void OnSessionStartPlay(std::shared_ptr<RtspSession> session) override
    {
        std::string session_id = session->GetSessionId();
        std::cout << "Session start play: " << session_id << std::endl;

        // Get media stream info to determine file path
        auto stream_info = session->GetMediaStreamInfo();
        if (!stream_info) {
            std::cout << "No media stream info for session: " << session_id << std::endl;
            return;
        }

        std::string stream_path = stream_info->stream_path;

        // Find corresponding file path
        std::lock_guard<std::mutex> lock(g_media_mutex);
        auto it = g_media_files.find(stream_path);
        if (it == g_media_files.end()) {
            std::cout << "Media file not found for stream: " << stream_path << std::endl;
            return;
        }

        const MediaFile &media = it->second;
        uint32_t frame_rate = stream_info->frame_rate > 0 ? stream_info->frame_rate : 25;

        // Start worker thread for this session
        if (!SessionManager::GetInstance().StartSession(session, media.file_path, frame_rate)) {
            std::cout << "Failed to start worker thread for session: " << session_id << std::endl;
        }
    }

    void OnSessionStopPlay(const std::string &session_id) override
    {
        std::cout << "Session stop play: " << session_id << std::endl;
        // Stop the worker thread for this session
        SessionManager::GetInstance().StopSession(session_id);
    }

    void OnPlayReceived(const std::string &client_ip, const std::string &stream_path,
                        const std::string &range = "") override
    {
        std::cout << "PLAY received from " << client_ip << " for " << stream_path << std::endl;
    }

    void OnPauseReceived(const std::string &client_ip, const std::string &stream_path) override
    {
        std::cout << "PAUSE received from " << client_ip << " for " << stream_path << std::endl;
    }

    void OnTeardownReceived(const std::string &client_ip, const std::string &stream_path) override
    {
        std::cout << "TEARDOWN received from " << client_ip << " for " << stream_path << std::endl;
    }

    void OnClientConnected(const std::string &client_ip, const std::string &user_agent) override
    {
        std::cout << "Client connected: " << client_ip << " (" << user_agent << ")" << std::endl;
    }

    void OnClientDisconnected(const std::string &client_ip) override
    {
        std::cout << "Client disconnected: " << client_ip << std::endl;
    }

    void OnStreamRequested(const std::string &stream_path, const std::string &client_ip) override
    {
        std::cout << "Stream requested: " << stream_path << " from " << client_ip << std::endl;
    }

    void OnSetupReceived(const std::string &client_ip, const std::string &transport,
                         const std::string &stream_path) override
    {
        std::cout << "SETUP received from " << client_ip << " for " << stream_path << " (transport: " << transport
                  << ")" << std::endl;
    }
};

// Signal handler
void SignalHandler(int signum)
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
    std::string ext = std::filesystem::path(filename).extension().string();
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
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        std::cerr << "Error: Media directory does not exist or is not a directory: " << directory << std::endl;
        return false;
    }

    std::cout << "\n=== Scanning media directory: " << directory << " ===" << std::endl;

    int fileCount = 0;

    try {
        for (const auto &entry : std::filesystem::directory_iterator(directory)) {
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

            // For H.264 files, load parameters using MappedFile
            if (codec == "H264") {
                // Use FileManager to get shared MappedFile
                auto mapped_file = FileManager::GetInstance().GetMappedFile(filepath);
                if (!mapped_file) {
                    std::cerr << "Warning: Failed to map H.264 file: " << filepath << std::endl;
                    continue;
                }

                // Create temporary SessionH264Reader to extract parameters
                SessionH264Reader temp_reader(mapped_file);

                // Create and register media stream
                auto streamInfo = std::make_shared<MediaStreamInfo>();
                streamInfo->stream_path = streamPath;
                streamInfo->media_type = "video";
                streamInfo->codec = "H264";
                streamInfo->payload_type = 96;
                streamInfo->clock_rate = 90000;

                // Set default resolution (will be updated from SPS if available)
                streamInfo->width = 1920;
                streamInfo->height = 1080;
                streamInfo->frame_rate = temp_reader.GetFrameRate();
                streamInfo->sps = temp_reader.GetSPS();
                streamInfo->pps = temp_reader.GetPPS();

                if (!g_server->AddMediaStream(streamPath, streamInfo)) {
                    std::cerr << "Warning: Failed to register stream: " << streamPath << std::endl;
                    FileManager::GetInstance().ReleaseMappedFile(filepath);
                    continue;
                }

                // Calculate duration from frame index
                auto playback_info = temp_reader.GetPlaybackInfo();
                double duration = playback_info.total_duration_;

                std::cout << "  [" << ++fileCount << "] " << filename << std::endl;
                std::cout << "      Stream:     rtsp://localhost:8554" << streamPath << std::endl;
                std::cout << "      Codec:      " << codec << std::endl;
                std::cout << "      Resolution: " << streamInfo->width << "x" << streamInfo->height << std::endl;
                std::cout << "      Frame rate: " << streamInfo->frame_rate << " fps" << std::endl;
                std::cout << "      Duration:   " << duration << " seconds" << std::endl;
                std::cout << "      Frames:     " << playback_info.total_frames_ << std::endl;

                // Release temporary reference
                FileManager::GetInstance().ReleaseMappedFile(filepath);
            }
            // Add support for other codecs here (AAC, TS)

            std::lock_guard<std::mutex> lock(g_media_mutex);
            g_media_files[streamPath] = media;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return false;
    }

    std::cout << "\n=== Found " << fileCount << " media file(s) ===" << std::endl;
    return fileCount > 0;
}

void PrintUsage(const char *programName)
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
        PrintUsage(argv[0]);
        return 1;
    }

    // Check for help
    std::string firstArg = argv[1];
    if (firstArg == "-h" || firstArg == "--help") {
        PrintUsage(argv[0]);
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
            PrintUsage(argv[0]);
            return 1;
        }

        argIndex++;
    }

    if (g_media_directory.empty()) {
        std::cerr << "Error: Media directory not specified\n" << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }

    // Register signal handler
    signal(SIGINT, SignalHandler);

    std::cout << "=== RTSP VOD Server ===" << std::endl;
    std::cout << "Listening on: " << ip << ":" << port << std::endl;
    std::cout << "Media directory: " << g_media_directory << std::endl;

    // Get server instance
    g_server = RtspServer::GetInstance();

    // Set session event callback
    auto callback = std::make_shared<SessionEventCallback>();
    g_server->SetCallback(callback);

    // Initialize server
    if (!g_server->Init(ip, port)) {
        std::cerr << "Failed to initialize RTSP server" << std::endl;
        if (g_server) {
            g_server->Stop();
        }
        return 1;
    }

    // Scan and register media files
    if (!ScanMediaDirectory(g_media_directory)) {
        std::cerr << "No media files found or failed to register streams" << std::endl;
        if (g_server) {
            g_server->Stop();
        }
        return 1;
    }

    // Start server
    if (!g_server->Start()) {
        std::cerr << "Failed to start RTSP server" << std::endl;
        if (g_server) {
            g_server->Stop();
        }
        return 1;
    }

    std::cout << "\n=== Server is running, press Ctrl+C to stop ===" << std::endl;

    // Main loop - monitor sessions and cleanup
    while (g_running) {
        // Cleanup finished sessions periodically
        size_t cleaned = SessionManager::GetInstance().CleanupFinishedSessions();
        if (cleaned > 0) {
            std::cout << "Cleaned up " << cleaned << " finished sessions" << std::endl;
        }

        // Print session statistics every 30 seconds
        static auto last_stats_time = std::chrono::steady_clock::now();
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_stats_time);

        if (elapsed.count() >= 30) {
            size_t active_count = SessionManager::GetInstance().GetActiveSessionCount();
            size_t cached_files = FileManager::GetInstance().GetCachedFileCount();

            std::cout << "Server stats - Active sessions: " << active_count << ", Cached files: " << cached_files
                      << std::endl;

            last_stats_time = current_time;
        }

        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    std::cout << "\nShutting down..." << std::endl;

    // Stop all session worker threads
    SessionManager::GetInstance().StopAllSessions();

    // Clear file cache
    FileManager::GetInstance().ClearCache();

    {
        std::lock_guard<std::mutex> lock(g_media_mutex);
        g_media_files.clear();
    }

    if (g_server) {
        g_server->Stop();
    }

    std::cout << "Server stopped" << std::endl;
    return 0;
}
