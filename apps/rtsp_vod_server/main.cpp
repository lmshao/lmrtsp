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
#include <lmrtsp/rtsp_server_session.h>
#include <signal.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <vector>
// Network interfaces enumeration for IPv4 addresses
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

#include "aac_file_reader.h"
#include "file_manager.h"
#include "session_aac_worker_thread.h"
#include "session_manager.h"
#include "session_ts_reader.h"
#include "session_ts_worker_thread.h"

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

// TS worker threads (separate from H264 workers managed by SessionManager)
std::map<std::string, std::shared_ptr<SessionTSWorkerThread>> g_ts_workers;
std::mutex g_ts_workers_mutex;

// AAC worker threads
std::map<std::string, std::shared_ptr<SessionAacWorkerThread>> g_aac_workers;
std::mutex g_aac_workers_mutex;

// Session event callback for managing worker threads
class SessionEventCallback : public lmshao::lmrtsp::IRtspServerCallback {
public:
    void OnSessionCreated(std::shared_ptr<RtspServerSession> session) override
    {
        std::cout << "Session created: " << session->GetSessionId() << std::endl;
    }

    void OnSessionDestroyed(const std::string &session_id) override
    {
        std::cout << "Session destroyed: " << session_id << std::endl;
        // Stop the H264 worker thread for this session
        SessionManager::GetInstance().StopSession(session_id);

        // Stop the TS worker thread if exists
        {
            std::lock_guard<std::mutex> ts_lock(g_ts_workers_mutex);
            auto ts_it = g_ts_workers.find(session_id);
            if (ts_it != g_ts_workers.end()) {
                ts_it->second->Stop();
                g_ts_workers.erase(ts_it);
                std::cout << "Stopped TS worker for session: " << session_id << std::endl;
            }
        }

        // Stop the AAC worker thread if exists
        {
            std::lock_guard<std::mutex> aac_lock(g_aac_workers_mutex);
            auto aac_it = g_aac_workers.find(session_id);
            if (aac_it != g_aac_workers.end()) {
                aac_it->second->Stop();
                g_aac_workers.erase(aac_it);
                std::cout << "Stopped AAC worker for session: " << session_id << std::endl;
            }
        }
    }

    void OnSessionStartPlay(std::shared_ptr<RtspServerSession> session) override
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

        // Handle different codecs
        if (media.codec == "H264") {
            uint32_t frame_rate = stream_info->frame_rate > 0 ? stream_info->frame_rate : 25;

            // Start H264 worker thread for this session
            if (!SessionManager::GetInstance().StartSession(session, media.file_path, frame_rate)) {
                std::cout << "Failed to start H264 worker thread for session: " << session_id << std::endl;
            }
        } else if (media.codec == "MP2T") {
            // Start TS worker thread for this session
            uint32_t bitrate = 2000000; // 2 Mbps default, could be read from stream_info if available

            auto ts_worker = std::make_shared<SessionTSWorkerThread>(session, media.file_path, bitrate);
            if (ts_worker->Start()) {
                std::lock_guard<std::mutex> ts_lock(g_ts_workers_mutex);
                g_ts_workers[session_id] = ts_worker;
                std::cout << "Started TS worker thread for session: " << session_id << std::endl;
            } else {
                std::cout << "Failed to start TS worker thread for session: " << session_id << std::endl;
            }
        } else if (media.codec == "AAC") {
            // Start AAC worker thread for this session
            uint32_t sample_rate = stream_info->sample_rate > 0 ? stream_info->sample_rate : 48000;

            auto aac_worker = std::make_shared<SessionAacWorkerThread>(session, media.file_path, sample_rate);
            if (aac_worker->Start()) {
                std::lock_guard<std::mutex> aac_lock(g_aac_workers_mutex);
                g_aac_workers[session_id] = aac_worker;
                std::cout << "Started AAC worker thread for session: " << session_id << std::endl;
            } else {
                std::cout << "Failed to start AAC worker thread for session: " << session_id << std::endl;
            }
        } else {
            std::cout << "Unsupported codec: " << media.codec << " for session: " << session_id << std::endl;
        }
    }

    void OnSessionStopPlay(const std::string &session_id) override
    {
        std::cout << "Session stop play: " << session_id << std::endl;
        // Stop the H264 worker thread for this session
        SessionManager::GetInstance().StopSession(session_id);

        // Stop the TS worker thread if exists
        {
            std::lock_guard<std::mutex> ts_lock(g_ts_workers_mutex);
            auto ts_it = g_ts_workers.find(session_id);
            if (ts_it != g_ts_workers.end()) {
                ts_it->second->Stop();
                g_ts_workers.erase(ts_it);
                std::cout << "Stopped TS worker for session: " << session_id << std::endl;
            }
        }

        // Stop the AAC worker thread if exists
        {
            std::lock_guard<std::mutex> aac_lock(g_aac_workers_mutex);
            auto aac_it = g_aac_workers.find(session_id);
            if (aac_it != g_aac_workers.end()) {
                aac_it->second->Stop();
                g_aac_workers.erase(aac_it);
                std::cout << "Stopped AAC worker for session: " << session_id << std::endl;
            }
        }
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

// Enumerate local IPv4 addresses (loopback included)
std::vector<std::string> EnumerateLocalIPv4()
{
    std::vector<std::string> ips;
    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        return ips;
    }

    for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)
            continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            auto *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            char buf[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &(sa->sin_addr), buf, sizeof(buf)) != nullptr) {
                std::string ip(buf);
                // Deduplicate
                if (std::find(ips.begin(), ips.end(), ip) == ips.end()) {
                    ips.push_back(ip);
                }
            }
        }
    }

    freeifaddrs(ifaddr);
    return ips;
}

// Print prominent RTSP URLs for all local IPs and discovered streams
void PrintStartupUrls(const std::vector<std::string> &ips, uint16_t port)
{
    std::vector<std::string> stream_paths;
    {
        std::lock_guard<std::mutex> lock(g_media_mutex);
        for (const auto &pair : g_media_files) {
            stream_paths.push_back(pair.first);
        }
    }
    // Sort paths for consistent ordering
    std::sort(stream_paths.begin(), stream_paths.end());

    std::cout << "\n=== Available RTSP URLs ===" << std::endl;
    if (ips.empty()) {
        std::cout << "No local IPv4 addresses detected. Use localhost: rtsp://localhost:" << port << "/<stream>"
                  << std::endl;
        return;
    }

    if (stream_paths.empty()) {
        std::cout << "No media files found to serve." << std::endl;
        return;
    }

    for (const auto &path : stream_paths) {
        std::cout << "\nStream: " << path << std::endl;
        for (const auto &ip_addr : ips) {
            std::cout << "  rtsp://" << ip_addr << ":" << port << path << std::endl;
        }
    }
    std::cout << std::endl;
}

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
            // Support for TS files
            else if (codec == "MP2T") {
                // Use FileManager to get shared MappedFile
                auto mapped_file = FileManager::GetInstance().GetMappedFile(filepath);
                if (!mapped_file) {
                    std::cerr << "Warning: Failed to map TS file: " << filepath << std::endl;
                    continue;
                }

                // Create temporary SessionTSReader to extract information
                SessionTSReader temp_reader(mapped_file);

                // Create and register media stream
                auto streamInfo = std::make_shared<MediaStreamInfo>();
                streamInfo->stream_path = streamPath;
                streamInfo->media_type = "video"; // TS can contain both audio and video
                streamInfo->codec = "MP2T";
                streamInfo->payload_type = 33; // Static payload type for MP2T
                streamInfo->clock_rate = 90000;

                // TS doesn't have separate SPS/PPS
                streamInfo->width = 0;      // Unknown until parsed
                streamInfo->height = 0;     // Unknown until parsed
                streamInfo->frame_rate = 0; // Will use packet-based timing

                if (!g_server->AddMediaStream(streamPath, streamInfo)) {
                    std::cerr << "Warning: Failed to register stream: " << streamPath << std::endl;
                    FileManager::GetInstance().ReleaseMappedFile(filepath);
                    continue;
                }

                // Get playback info
                auto playback_info = temp_reader.GetPlaybackInfo();
                double duration = playback_info.total_duration_;
                uint32_t bitrate = temp_reader.GetBitrate();

                std::cout << "  [" << ++fileCount << "] " << filename << std::endl;
                std::cout << "      Stream:     rtsp://localhost:8554" << streamPath << std::endl;
                std::cout << "      Codec:      " << codec << " (MPEG-TS)" << std::endl;
                std::cout << "      Bitrate:    " << (bitrate / 1000000.0) << " Mbps" << std::endl;
                std::cout << "      Duration:   " << duration << " seconds" << std::endl;
                std::cout << "      Packets:    " << playback_info.total_packets_ << std::endl;

                // Release temporary reference
                FileManager::GetInstance().ReleaseMappedFile(filepath);
            }
            // Support for AAC files
            else if (codec == "AAC") {
                // Use FileManager to get shared MappedFile
                auto mapped_file = FileManager::GetInstance().GetMappedFile(filepath);
                if (!mapped_file) {
                    std::cerr << "Warning: Failed to map AAC file: " << filepath << std::endl;
                    continue;
                }

                // Create temporary AacFileReader to extract information
                AacFileReader temp_reader(mapped_file);
                if (!temp_reader.IsValid()) {
                    std::cerr << "Warning: Invalid AAC file: " << filepath << std::endl;
                    FileManager::GetInstance().ReleaseMappedFile(filepath);
                    continue;
                }

                // Create and register media stream
                auto streamInfo = std::make_shared<MediaStreamInfo>();
                streamInfo->stream_path = streamPath;
                streamInfo->media_type = "audio";
                streamInfo->codec = "AAC";
                streamInfo->payload_type = 97; // Dynamic payload type for AAC
                streamInfo->sample_rate = temp_reader.GetSampleRate();
                streamInfo->channels = temp_reader.GetChannels();
                streamInfo->clock_rate = temp_reader.GetSampleRate();

                if (!g_server->AddMediaStream(streamPath, streamInfo)) {
                    std::cerr << "Warning: Failed to register stream: " << streamPath << std::endl;
                    FileManager::GetInstance().ReleaseMappedFile(filepath);
                    continue;
                }

                // Get playback info
                auto playback_info = temp_reader.GetPlaybackInfo();
                double duration = playback_info.total_duration_;
                uint32_t bitrate = temp_reader.GetBitrate();

                std::cout << "  [" << ++fileCount << "] " << filename << std::endl;
                std::cout << "      Stream:     rtsp://localhost:8554" << streamPath << std::endl;
                std::cout << "      Codec:      " << codec << " (AAC-LC)" << std::endl;
                std::cout << "      Sample rate: " << streamInfo->sample_rate << " Hz" << std::endl;
                std::cout << "      Channels:   " << (int)streamInfo->channels << std::endl;
                std::cout << "      Bitrate:    " << (bitrate / 1000.0) << " kbps" << std::endl;
                std::cout << "      Duration:   " << duration << " seconds" << std::endl;
                std::cout << "      Frames:     " << playback_info.total_frames_ << std::endl;

                // Release temporary reference
                FileManager::GetInstance().ReleaseMappedFile(filepath);
            }

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

    // Print prominent URLs for all local IPs (if bound to 0.0.0.0) or the bound IP
    std::vector<std::string> ips;
    if (ip == "0.0.0.0") {
        ips = EnumerateLocalIPv4();
    } else {
        ips.push_back(ip);
    }
    PrintStartupUrls(ips, port);

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

    // Stop all session worker threads (H264)
    SessionManager::GetInstance().StopAllSessions();

    // Stop all TS worker threads
    {
        std::lock_guard<std::mutex> ts_lock(g_ts_workers_mutex);
        for (auto &pair : g_ts_workers) {
            std::cout << "Stopping TS worker: " << pair.first << std::endl;
            pair.second->Stop();
        }
        g_ts_workers.clear();
    }

    // Stop all AAC worker threads
    {
        std::lock_guard<std::mutex> aac_lock(g_aac_workers_mutex);
        for (auto &pair : g_aac_workers) {
            std::cout << "Stopping AAC worker: " << pair.first << std::endl;
            pair.second->Stop();
        }
        g_aac_workers.clear();
    }

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
