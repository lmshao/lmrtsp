/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_MKV_WORKER_THREAD_H
#define LMSHAO_RTSP_SESSION_MKV_WORKER_THREAD_H

#include <lmcore/data_buffer.h>
#include <lmrtsp/media_types.h>
#include <lmrtsp/rtsp_server_session.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "file_manager.h"
#include "session_mkv_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling MKV streaming
 */
class SessionMkvWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to handle
     * @param file_path Path to the MKV file
     * @param track_number Target track number
     * @param frame_rate Target frame rate for streaming (video) or sample rate (audio)
     */
    SessionMkvWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                           uint64_t track_number, int rtsp_track_index, uint32_t frame_rate = 25);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~SessionMkvWorkerThread();

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
    SessionMkvReader::PlaybackInfo GetPlaybackInfo() const;

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
     * @brief Send next frame to client
     * @return true if frame sent successfully, false if EOF or error
     */
    bool SendNextFrame();

    /**
     * @brief Calculate frame interval based on frame rate
     * @return Frame interval in microseconds
     */
    std::chrono::microseconds GetFrameInterval() const;

    /**
     * @brief Check if session is still valid and playing
     * @return true if session is active and playing, false otherwise
     */
    bool IsSessionActive() const;

    /**
     * @brief Determine media type from codec ID
     */
    MediaType GetMediaType() const;

private:
    // Session management
    std::shared_ptr<RtspServerSession> session_;
    std::string session_id_;
    std::string file_path_;
    uint64_t track_number_;
    int rtsp_track_index_; // RTSP track index (0, 1, ...) for PushFrame

    // MKV reader for independent playback
    std::unique_ptr<SessionMkvReader> mkv_reader_;

    // Thread management
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;

    // Streaming parameters
    std::atomic<uint32_t> frame_rate_; // frames per second (video) or samples per second (audio)
    std::atomic<uint64_t> frame_counter_;

    // Timing control
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;

    // Statistics
    std::atomic<size_t> frames_sent_;
    std::atomic<size_t> bytes_sent_;
};

#endif // LMSHAO_RTSP_SESSION_MKV_WORKER_THREAD_H
