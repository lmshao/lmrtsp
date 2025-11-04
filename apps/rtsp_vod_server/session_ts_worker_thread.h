/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_TS_WORKER_THREAD_H
#define LMSHAO_RTSP_SESSION_TS_WORKER_THREAD_H

#include <lmcore/data_buffer.h>
#include <lmrtsp/media_types.h>
#include <lmrtsp/rtsp_session.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "file_manager.h"
#include "session_ts_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling TS (MPEG-TS) streaming
 */
class SessionTSWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to handle
     * @param file_path Path to the TS file
     * @param bitrate Target bitrate for streaming (bps)
     */
    SessionTSWorkerThread(std::shared_ptr<RtspSession> session, const std::string &file_path,
                          uint32_t bitrate = 2000000);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~SessionTSWorkerThread();

    /**
     * @brief Start the worker thread
     * @return true if started successfully, false otherwise
     */
    bool Start();

    /**
     * @brief Stop the worker thread
     */
    void Stop();

    /**
     * @brief Check if the worker thread is running
     * @return true if running, false otherwise
     */
    bool IsRunning() const;

    /**
     * @brief Get session ID for identification
     * @return Session ID string
     */
    std::string GetSessionId() const;

    /**
     * @brief Get current playback information
     * @return Playback info structure
     */
    SessionTSReader::PlaybackInfo GetPlaybackInfo() const;

    /**
     * @brief Reset playback to beginning
     */
    void Reset();

private:
    /**
     * @brief Main worker thread function
     */
    void WorkerThreadFunc();

    /**
     * @brief Send next TS packet to client
     * @return true if packet sent successfully, false if EOF or error
     */
    bool SendNextPacket();

    /**
     * @brief Calculate packet interval based on bitrate
     * @return Packet interval in microseconds
     */
    std::chrono::microseconds GetPacketInterval() const;

    /**
     * @brief Check if session is still valid and playing
     * @return true if session is active and playing, false otherwise
     */
    bool IsSessionActive() const;

private:
    // Session management
    std::shared_ptr<RtspSession> session_;
    std::string session_id_;
    std::string file_path_;

    // TS reader for independent playback
    std::unique_ptr<SessionTSReader> ts_reader_;

    // Thread management
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;

    // Streaming parameters
    std::atomic<uint32_t> bitrate_; // bits per second
    std::atomic<uint64_t> packet_counter_;

    // Timing control
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_packet_time_;

    // Statistics
    std::atomic<size_t> packets_sent_;
    std::atomic<size_t> bytes_sent_;
};

#endif // LMSHAO_RTSP_SESSION_TS_WORKER_THREAD_H
