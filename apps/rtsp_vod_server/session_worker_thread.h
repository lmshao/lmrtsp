/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_WORKER_THREAD_H
#define LMSHAO_RTSP_SESSION_WORKER_THREAD_H

#include <lmcore/data_buffer.h>
#include <lmrtsp/media_types.h>
#include <lmrtsp/rtsp_session.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "file_manager.h"
#include "session_h264_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling individual RTSP client session
 *
 * Each SessionWorkerThread manages one client session independently:
 * - Uses shared MappedFile through FileManager for efficient memory usage
 * - Maintains independent playback progress with SessionH264Reader
 * - Runs in its own thread for concurrent client support
 * - Handles frame timing and streaming control
 */
class SessionWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to handle
     * @param file_path Path to the H.264 file
     * @param frame_rate Target frame rate for streaming (fps)
     */
    SessionWorkerThread(std::shared_ptr<RtspSession> session, const std::string &file_path, uint32_t frame_rate = 25);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~SessionWorkerThread();

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
    SessionH264Reader::PlaybackInfo GetPlaybackInfo() const;

    /**
     * @brief Seek to specific frame
     * @param frame_index Target frame index
     * @return true if seek successful, false otherwise
     */
    bool SeekToFrame(size_t frame_index);

    /**
     * @brief Seek to specific time
     * @param timestamp Target timestamp in seconds
     * @return true if seek successful, false otherwise
     */
    bool SeekToTime(double timestamp);

    /**
     * @brief Reset playback to beginning
     */
    void Reset();

    /**
     * @brief Set frame rate for streaming
     * @param fps Frames per second
     */
    void SetFrameRate(uint32_t fps);

    /**
     * @brief Get current frame rate
     * @return Frames per second
     */
    uint32_t GetFrameRate() const;

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
     * @return Frame interval in milliseconds
     */
    std::chrono::milliseconds GetFrameInterval() const;

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

    // H.264 reader for independent playback
    std::unique_ptr<SessionH264Reader> h264_reader_;

    // Thread management
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;

    // Streaming parameters
    std::atomic<uint32_t> frame_rate_;
    std::atomic<uint64_t> frame_counter_;

    // Timing control
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;

    // Statistics
    std::atomic<size_t> frames_sent_;
    std::atomic<size_t> bytes_sent_;
};

#endif // LMSHAO_RTSP_SESSION_WORKER_THREAD_H