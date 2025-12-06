/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_BASE_SESSION_WORKER_THREAD_H
#define LMSHAO_RTSP_BASE_SESSION_WORKER_THREAD_H

#include <lmrtsp/rtsp_server_session.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace lmshao::lmrtsp;

/**
 * @brief Base class for all session worker threads
 *
 * This class provides common functionality for managing worker threads:
 * - Thread lifecycle management (Start, Stop, IsRunning)
 * - Session management (GetSessionId, IsSessionActive)
 * - Common state tracking (running, should_stop, statistics)
 *
 * Derived classes should implement:
 * - InitializeReader() - Initialize the specific reader
 * - SendNextData() - Send next frame/packet to client
 * - GetDataInterval() - Calculate interval between data units
 * - ResetReader() - Reset the reader to beginning
 */
class BaseSessionWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to handle
     * @param file_path Path to the media file
     */
    BaseSessionWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    virtual ~BaseSessionWorkerThread();

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
    bool IsRunning() const { return running_.load(); }

    /**
     * @brief Get session ID for identification
     * @return Session ID string
     */
    std::string GetSessionId() const { return session_id_; }

    /**
     * @brief Reset playback to beginning
     */
    virtual void Reset() = 0;

protected:
    /**
     * @brief Main worker thread function (template method pattern)
     */
    void WorkerThreadFunc();

    /**
     * @brief Check if session is still valid and playing
     * @return true if session is active and playing, false otherwise
     */
    bool IsSessionActive() const;

    /**
     * @brief Initialize the reader (called by Start())
     * @return true if successful, false otherwise
     */
    virtual bool InitializeReader() = 0;

    /**
     * @brief Send next data unit (frame/packet) to client
     * @return true if sent successfully, false if EOF or error
     */
    virtual bool SendNextData() = 0;

    /**
     * @brief Get interval between data units
     * @return Interval duration
     */
    virtual std::chrono::microseconds GetDataInterval() const = 0;

    /**
     * @brief Reset the reader to beginning
     */
    virtual void ResetReader() = 0;

    /**
     * @brief Cleanup reader resources (called by Stop())
     */
    virtual void CleanupReader() = 0;

    /**
     * @brief Release file resources (called by Stop())
     */
    virtual void ReleaseFile() = 0;

    /**
     * @brief Handle end of file (called when SendNextData returns false)
     */
    virtual void HandleEOF();

    // Session management
    std::shared_ptr<RtspServerSession> session_;
    std::string session_id_;
    std::string file_path_;

    // Thread management
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;

    // Timing control
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_data_time_;

    // Statistics
    std::atomic<size_t> data_sent_;
    std::atomic<size_t> bytes_sent_;
};

#endif // LMSHAO_RTSP_BASE_SESSION_WORKER_THREAD_H
