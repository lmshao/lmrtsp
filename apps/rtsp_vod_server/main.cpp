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

#include <lmnet/network_utils.h>
#include <lmrtsp/lmrtsp_logger.h>
#include <lmrtsp/media_stream_info.h>
#include <lmrtsp/rtsp_server.h>
#include <lmrtsp/rtsp_server_session.h>
#include <signal.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include "aac_file_reader.h"
#include "file_manager.h"
#include "session_aac_worker_thread.h"
#include "session_h265_reader.h"
#include "session_h265_worker_thread.h"
#include "session_manager.h"
#include "session_mkv_reader.h"
#include "session_mkv_worker_thread.h"
#include "session_ts_reader.h"
#include "session_ts_worker_thread.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;
namespace lmnet = lmshao::lmnet;

// Media file manager
struct MediaFile {
    std::string filename;
    std::string stream_path;   // RTSP URL path
    std::string file_path;     // Full file path
    std::string codec;         // H264, H265, AAC, MP2T, MKV
    uint64_t track_number = 0; // For MKV files (0 = not used)
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

// H265 worker threads
std::map<std::string, std::shared_ptr<SessionH265WorkerThread>> g_h265_workers;
std::mutex g_h265_workers_mutex;

// MKV worker threads
std::map<std::string, std::shared_ptr<SessionMkvWorkerThread>> g_mkv_workers;
std::mutex g_mkv_workers_mutex;

// Session event listener for managing worker threads
class SessionEventListener : public lmshao::lmrtsp::IRtspServerListener {
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

        // Stop the H265 worker thread if exists
        {
            std::lock_guard<std::mutex> h265_lock(g_h265_workers_mutex);
            auto h265_it = g_h265_workers.find(session_id);
            if (h265_it != g_h265_workers.end()) {
                h265_it->second->Stop();
                g_h265_workers.erase(h265_it);
                std::cout << "Stopped H265 worker for session: " << session_id << std::endl;
            }
        }

        // Stop the MKV worker thread if exists
        {
            std::lock_guard<std::mutex> mkv_lock(g_mkv_workers_mutex);
            auto mkv_it = g_mkv_workers.find(session_id);
            if (mkv_it != g_mkv_workers.end()) {
                mkv_it->second->Stop();
                g_mkv_workers.erase(mkv_it);
                std::cout << "Stopped MKV worker for session: " << session_id << std::endl;
            }
        }
    }

    void OnSessionStartPlay(std::shared_ptr<RtspServerSession> session) override
    {
        std::string session_id = session->GetSessionId();
        std::cout << "Session start play: " << session_id << std::endl;

        // Write to debug file to avoid output corruption
        std::ofstream debug("/tmp/vod_debug.txt", std::ios::app);
        debug << "====== OnSessionStartPlay called ======" << std::endl;
        debug << "Session ID: " << session_id << std::endl;

        bool is_multi = session->IsMultiTrack();
        debug << "IsMultiTrack: " << (is_multi ? "TRUE" : "FALSE") << std::endl;
        std::cout << "DEBUG: IsMultiTrack = " << (is_multi ? "TRUE" : "FALSE") << std::endl;

        // Check if this is a multi-track session
        if (is_multi) {
            debug << "Entering multi-track branch" << std::endl;
            std::cout << "Multi-track session detected" << std::endl;
            auto tracks = session->GetTracks();
            std::cout << "Starting " << tracks.size() << " workers for multi-track session" << std::endl;

            for (const auto &track : tracks) {
                std::cout << "  Track " << track.track_index << ": " << track.uri << std::endl;

                if (!track.stream_info) {
                    std::cout << "  Warning: No stream info for track " << track.track_index << std::endl;
                    continue;
                }

                // Extract path from URI (e.g., rtsp://host/path -> /path)
                std::string track_path = track.uri;
                size_t path_start = track_path.find("://");
                if (path_start != std::string::npos) {
                    path_start = track_path.find('/', path_start + 3);
                    if (path_start != std::string::npos) {
                        track_path = track_path.substr(path_start);
                    }
                }

                // Map track0 -> track1, track1 -> track2 (RTSP vs MKV track numbering)
                // In RTSP SDP, tracks are numbered from 0
                // But in g_media_files, they use MKV internal track numbers (usually starting from 1)
                size_t track_pos = track_path.rfind("/track");
                if (track_pos != std::string::npos) {
                    std::string base_path = track_path.substr(0, track_pos);
                    int rtsp_track_idx = track.track_index;

                    // Find the MKV track number from stream_info
                    std::string mkv_track_path = track.stream_info->stream_path;
                    std::cout << "  MKV stream path: " << mkv_track_path << std::endl;
                    track_path = mkv_track_path;
                }

                // Find the media file for this track
                std::lock_guard<std::mutex> lock(g_media_mutex);
                auto it = g_media_files.find(track_path);
                if (it == g_media_files.end()) {
                    std::cout << "  Warning: Media file not found for track path: " << track_path << std::endl;
                    continue;
                }

                const MediaFile &media = it->second;

                // Start worker for this track
                if (media.codec == "MKV") {
                    uint32_t frame_rate;

                    // Calculate correct frame rate based on media type
                    if (track.stream_info->media_type == "video") {
                        // Video: use configured frame rate
                        frame_rate = track.stream_info->frame_rate > 0 ? track.stream_info->frame_rate : 25;
                        std::cout << "  DEBUG: Video track, frame_rate=" << frame_rate << std::endl;
                    } else if (track.stream_info->media_type == "audio") {
                        // Audio: calculate frame rate from sample rate
                        // AAC frame size is typically 1024 samples
                        // Frame rate = sample_rate / samples_per_frame
                        uint32_t sample_rate = track.stream_info->sample_rate;
                        uint32_t samples_per_frame = 1024; // AAC-LC standard
                        if (sample_rate > 0) {
                            frame_rate = (sample_rate * 1000) / samples_per_frame; // *1000 for better precision
                        } else {
                            frame_rate = 46875; // Default: 48000Hz / 1024 * 1000 = 46.875 fps * 1000
                        }
                        std::cout << "  DEBUG: Audio track, sample_rate=" << sample_rate
                                  << ", frame_rate=" << frame_rate << std::endl;
                    } else {
                        frame_rate = 25; // Fallback
                        std::cout << "  DEBUG: Unknown media_type='" << track.stream_info->media_type
                                  << "', frame_rate=" << frame_rate << std::endl;
                    }

                    // Create unique worker key for multi-track: session_id + track index
                    std::string worker_key = session_id + "_track" + std::to_string(track.track_index);

                    auto mkv_worker = std::make_shared<SessionMkvWorkerThread>(
                        session, media.file_path, media.track_number, track.track_index, frame_rate);
                    if (mkv_worker->Start()) {
                        std::lock_guard<std::mutex> mkv_lock(g_mkv_workers_mutex);
                        g_mkv_workers[worker_key] = mkv_worker;
                        std::cout << "  Started MKV worker for track " << track.track_index << " (file track "
                                  << media.track_number << ", " << track.stream_info->codec << ", rate=" << frame_rate
                                  << ")" << std::endl;
                    } else {
                        std::cout << "  Failed to start MKV worker for track " << track.track_index << std::endl;
                    }
                } else {
                    std::cout << "  Unsupported codec for multi-track: " << media.codec << std::endl;
                }
            }

            std::cout << "Multi-track workers started for session: " << session_id << std::endl;
            return;
        }

        // Single-track session (legacy mode)
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
        } else if (media.codec == "H265") {
            // Start H265 worker thread for this session
            uint32_t frame_rate = stream_info->frame_rate > 0 ? stream_info->frame_rate : 25;

            auto h265_worker = std::make_shared<SessionH265WorkerThread>(session, media.file_path, frame_rate);
            if (h265_worker->Start()) {
                std::lock_guard<std::mutex> h265_lock(g_h265_workers_mutex);
                g_h265_workers[session_id] = h265_worker;
                std::cout << "Started H265 worker thread for session: " << session_id << std::endl;
            } else {
                std::cout << "Failed to start H265 worker thread for session: " << session_id << std::endl;
            }
        } else if (media.codec == "MKV") {
            // Start MKV worker thread for this session
            uint32_t frame_rate = stream_info->frame_rate > 0 ? stream_info->frame_rate : 25;

            auto mkv_worker =
                std::make_shared<SessionMkvWorkerThread>(session, media.file_path, media.track_number, frame_rate);
            if (mkv_worker->Start()) {
                std::lock_guard<std::mutex> mkv_lock(g_mkv_workers_mutex);
                g_mkv_workers[session_id] = mkv_worker;
                std::cout << "Started MKV worker thread for session: " << session_id << " (track " << media.track_number
                          << ")" << std::endl;
            } else {
                std::cout << "Failed to start MKV worker thread for session: " << session_id << std::endl;
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

// Enumerate local IPv4 addresses (using lmnet cross-platform implementation)
std::vector<std::string> EnumerateLocalIPv4()
{
    return lmnet::NetworkUtils::GetAllIPv4Addresses();
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
    } else if (ext == ".h265" || ext == ".265" || ext == ".hevc") {
        return "H265";
    } else if (ext == ".aac") {
        return "AAC";
    } else if (ext == ".ts" || ext == ".m2ts") {
        return "MP2T";
    } else if (ext == ".mkv" || ext == ".webm") {
        return "MKV";
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
            // Support for H265 files
            else if (codec == "H265") {
                auto mapped_file = FileManager::GetInstance().GetMappedFile(filepath);
                if (!mapped_file) {
                    std::cerr << "Warning: Failed to map H.265 file: " << filepath << std::endl;
                    continue;
                }

                SessionH265Reader temp_reader(mapped_file);

                auto streamInfo = std::make_shared<MediaStreamInfo>();
                streamInfo->stream_path = streamPath;
                streamInfo->media_type = "video";
                streamInfo->codec = "H265";
                streamInfo->payload_type = 98;
                streamInfo->clock_rate = 90000;

                streamInfo->width = 1920;
                streamInfo->height = 1080;
                streamInfo->frame_rate = temp_reader.GetFrameRate();
                streamInfo->vps = temp_reader.GetVPS();
                streamInfo->sps = temp_reader.GetSPS();
                streamInfo->pps = temp_reader.GetPPS();

                if (!g_server->AddMediaStream(streamPath, streamInfo)) {
                    std::cerr << "Warning: Failed to register stream: " << streamPath << std::endl;
                    FileManager::GetInstance().ReleaseMappedFile(filepath);
                    continue;
                }

                auto playback_info = temp_reader.GetPlaybackInfo();
                double duration = playback_info.total_duration_;

                std::cout << "  [" << ++fileCount << "] " << filename << std::endl;
                std::cout << "      Stream:     rtsp://localhost:8554" << streamPath << std::endl;
                std::cout << "      Codec:      " << codec << std::endl;
                std::cout << "      Resolution: " << streamInfo->width << "x" << streamInfo->height << std::endl;
                std::cout << "      Frame rate: " << streamInfo->frame_rate << " fps" << std::endl;
                std::cout << "      Duration:   " << duration << " seconds" << std::endl;
                std::cout << "      Frames:     " << playback_info.total_frames_ << std::endl;

                FileManager::GetInstance().ReleaseMappedFile(filepath);
            }
            // Support for MKV files
            else if (codec == "MKV") {
                auto mapped_file = FileManager::GetInstance().GetMappedFile(filepath);
                if (!mapped_file) {
                    std::cerr << "Warning: Failed to map MKV file: " << filepath << std::endl;
                    continue;
                }

                // Use MkvDemuxer to scan tracks
                lmshao::lmmkv::MkvDemuxer scanner;

                struct ScanListener : public lmshao::lmmkv::IMkvDemuxListener {
                    lmshao::lmmkv::MkvInfo info;
                    std::vector<lmshao::lmmkv::MkvTrackInfo> tracks;

                    void OnInfo(const lmshao::lmmkv::MkvInfo &i) override { info = i; }
                    void OnTrack(const lmshao::lmmkv::MkvTrackInfo &t) override { tracks.push_back(t); }
                    void OnFrame(const lmshao::lmmkv::MkvFrame &) override {}
                    void OnEndOfStream() override {}
                    void OnError(int, const std::string &) override {}
                };

                auto scan_listener = std::make_shared<ScanListener>();
                scanner.SetListener(scan_listener);

                if (!scanner.Start()) {
                    std::cerr << "Warning: Failed to start MKV scanner for: " << filepath << std::endl;
                    FileManager::GetInstance().ReleaseMappedFile(filepath);
                    continue;
                }

                // Quick scan - only parse headers
                const uint8_t *data = mapped_file->Data();
                size_t scan_size = std::min(mapped_file->Size(), size_t(1024 * 1024)); // Scan first 1MB
                scanner.Consume(data, scan_size);
                scanner.Stop();

                // Find first video track for main stream registration
                uint64_t default_video_track = 0;
                for (const auto &track : scan_listener->tracks) {
                    if (track.codec_id.find("V_MPEG4/ISO/AVC") == 0 || track.codec_id.find("V_MPEGH/ISO/HEVC") == 0) {
                        default_video_track = track.track_number;
                        break;
                    }
                }

                // Register streams for each supported track
                for (const auto &track : scan_listener->tracks) {
                    std::string track_codec;
                    MediaType media_type;

                    if (track.codec_id.find("V_MPEG4/ISO/AVC") == 0) {
                        track_codec = "H264";
                        media_type = MediaType::H264;
                    } else if (track.codec_id.find("V_MPEGH/ISO/HEVC") == 0) {
                        track_codec = "H265";
                        media_type = MediaType::H265;
                    } else if (track.codec_id.find("A_AAC") == 0) {
                        track_codec = "AAC";
                        media_type = MediaType::AAC;
                    } else {
                        // Skip unsupported codecs
                        std::cout << "      Skipping unsupported track " << track.track_number << " (" << track.codec_id
                                  << ")" << std::endl;
                        continue;
                    }

                    // Generate stream path: /filename.mkv/track{N}
                    std::string track_stream_path = "/" + filename + "/track" + std::to_string(track.track_number);

                    // Create MediaFile entry
                    MediaFile media;
                    media.filename = filename;
                    media.stream_path = track_stream_path;
                    media.file_path = filepath;
                    media.codec = "MKV"; // Mark as MKV container
                    media.track_number = track.track_number;

                    // Create and register media stream
                    auto streamInfo = std::make_shared<MediaStreamInfo>();
                    streamInfo->stream_path = track_stream_path;
                    streamInfo->codec = track_codec;
                    streamInfo->clock_rate = 90000;

                    if (track_codec == "H264" || track_codec == "H265") {
                        // Video track
                        streamInfo->media_type = "video";
                        streamInfo->payload_type = (track_codec == "H264") ? 96 : 98;
                        streamInfo->width = track.width > 0 ? track.width : 1920;
                        streamInfo->height = track.height > 0 ? track.height : 1080;

                        // Estimate frame rate from duration (will be refined during playback)
                        streamInfo->frame_rate = 25; // Default

                        // Extract parameter sets from codec_private
                        SessionMkvReader temp_reader(mapped_file, track.track_number);
                        if (temp_reader.Initialize()) {
                            streamInfo->frame_rate = temp_reader.GetFrameRate();

                            if (track_codec == "H264") {
                                streamInfo->sps = temp_reader.GetSPS();
                                streamInfo->pps = temp_reader.GetPPS();
                            } else {
                                streamInfo->vps = temp_reader.GetVPS();
                                streamInfo->sps = temp_reader.GetSPS();
                                streamInfo->pps = temp_reader.GetPPS();
                            }
                        }
                    } else if (track_codec == "AAC") {
                        // Audio track
                        streamInfo->media_type = "audio";
                        streamInfo->payload_type = 97;
                        streamInfo->sample_rate = track.sample_rate > 0 ? track.sample_rate : 48000;
                        streamInfo->channels = track.channels > 0 ? track.channels : 2;
                        streamInfo->clock_rate = streamInfo->sample_rate;
                    }

                    if (!g_server->AddMediaStream(track_stream_path, streamInfo)) {
                        std::cerr << "Warning: Failed to register MKV stream: " << track_stream_path << std::endl;
                        continue;
                    }

                    std::cout << "  [" << ++fileCount << "] " << filename << " - Track " << track.track_number
                              << std::endl;
                    std::cout << "      Stream:     rtsp://localhost:8554" << track_stream_path << std::endl;
                    std::cout << "      Codec:      " << track_codec << " (from MKV)" << std::endl;

                    if (streamInfo->media_type == "video") {
                        std::cout << "      Resolution: " << streamInfo->width << "x" << streamInfo->height
                                  << std::endl;
                        std::cout << "      Frame rate: " << streamInfo->frame_rate << " fps" << std::endl;
                    } else {
                        std::cout << "      Sample rate: " << streamInfo->sample_rate << " Hz" << std::endl;
                        std::cout << "      Channels:   " << (int)streamInfo->channels << std::endl;
                    }

                    std::cout << "      Duration:   " << scan_listener->info.duration_seconds << " seconds"
                              << std::endl;

                    // Store in global map
                    std::lock_guard<std::mutex> lock(g_media_mutex);
                    g_media_files[track_stream_path] = media;
                }

                // Register main stream with multi-track support
                if (!scan_listener->tracks.empty()) {
                    std::string main_stream_path = "/" + filename;

                    // Create main stream info with sub-tracks
                    auto main_stream_info = std::make_shared<MediaStreamInfo>();
                    main_stream_info->stream_path = main_stream_path;
                    // Use first track's type as main type (usually video)
                    main_stream_info->media_type = "multi";
                    main_stream_info->codec = "MKV";

                    // Collect all registered tracks as sub-tracks
                    for (const auto &track : scan_listener->tracks) {
                        // Find the registered stream info for this track
                        std::string track_stream_path = "/" + filename + "/track" + std::to_string(track.track_number);
                        auto track_info = g_server->GetMediaStream(track_stream_path);
                        if (track_info) {
                            main_stream_info->sub_tracks.push_back(track_info);
                        }
                    }

                    if (!main_stream_info->sub_tracks.empty()) {
                        if (g_server->AddMediaStream(main_stream_path, main_stream_info)) {
                            // Create MediaFile entry for main stream pointing to first video track
                            MediaFile main_media;
                            main_media.filename = filename;
                            main_media.stream_path = main_stream_path;
                            main_media.file_path = filepath;
                            main_media.codec = "MKV";
                            main_media.track_number = default_video_track;

                            std::lock_guard<std::mutex> lock(g_media_mutex);
                            g_media_files[main_stream_path] = main_media;

                            std::cout << "  [" << ++fileCount << "] " << filename << " (Multi-Track Stream)"
                                      << std::endl;
                            std::cout << "      Stream:     rtsp://localhost:8554" << main_stream_path << std::endl;
                            std::cout << "      Tracks:     " << main_stream_info->sub_tracks.size() << " (";
                            for (size_t i = 0; i < main_stream_info->sub_tracks.size(); ++i) {
                                if (i > 0)
                                    std::cout << ", ";
                                std::cout << main_stream_info->sub_tracks[i]->codec;
                            }
                            std::cout << ")" << std::endl;
                            std::cout << "      Note:       Multi-track RTSP streaming with synchronized A/V"
                                      << std::endl;
                        }
                    }
                }

                FileManager::GetInstance().ReleaseMappedFile(filepath);
            }
            // Note: For MKV files, we already registered all tracks and main stream above
            // So we don't need to add anything else to g_media_files here

            // For non-MKV files, register the media info
            if (codec != "MKV") {
                std::lock_guard<std::mutex> lock(g_media_mutex);
                g_media_files[streamPath] = media;
            }
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
    std::cout << "  media_directory  Directory containing media files (.h264, .h265, .aac, .ts, .mkv)" << std::endl;
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
    std::cout << "  For MKV file with multiple tracks, use: rtsp://server:8554/movie.mkv/track1" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "  ffplay rtsp://localhost:8554/movie.h264" << std::endl;
    std::cout << "  ffplay rtsp://localhost:8554/movie.mkv/track1" << std::endl;
    std::cout << "  vlc rtsp://localhost:8554/movie.h264" << std::endl;
    std::cout << "" << std::endl;

    std::cout << "Supported formats:" << std::endl;
    std::cout << "  Video: .h264, .264, .h265, .265, .hevc" << std::endl;
    std::cout << "  Audio: .aac" << std::endl;
    std::cout << "  Container: .ts, .m2ts, .mkv, .webm (H.264/H.265/AAC tracks)" << std::endl;
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

    // Set session event listener
    auto listener = std::make_shared<SessionEventListener>();
    g_server->SetListener(listener);

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

    // Stop all H265 worker threads
    {
        std::lock_guard<std::mutex> h265_lock(g_h265_workers_mutex);
        for (auto &pair : g_h265_workers) {
            std::cout << "Stopping H265 worker: " << pair.first << std::endl;
            pair.second->Stop();
        }
        g_h265_workers.clear();
    }

    // Stop all MKV worker threads
    {
        std::lock_guard<std::mutex> mkv_lock(g_mkv_workers_mutex);
        for (auto &pair : g_mkv_workers) {
            std::cout << "Stopping MKV worker: " << pair.first << std::endl;
            pair.second->Stop();
        }
        g_mkv_workers.clear();
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
