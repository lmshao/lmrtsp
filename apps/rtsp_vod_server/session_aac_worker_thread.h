/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SESSION_AAC_WORKER_THREAD_H
#define SESSION_AAC_WORKER_THREAD_H

#include <lmrtsp/rtsp_server_session.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "session_aac_reader.h"

/**
 * @brief Worker thread for AAC audio streaming
 *
 * Reads AAC frames from file and sends them via RTP at correct timing.
 * Uses frame duration (1024 samples per frame) for timing control.
 */
class SessionAacWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to send data to
     * @param file_path Path to AAC file
     * @param sample_rate Audio sample rate (e.g., 48000, 44100)
     */
    SessionAacWorkerThread(std::shared_ptr<lmshao::lmrtsp::RtspServerSession> session, const std::string &file_path,
                           uint32_t sample_rate);

    ~SessionAacWorkerThread();

    /**
     * @brief Start the worker thread
     * @return true if started successfully
     */
    bool Start();

    /**
     * @brief Stop the worker thread
     */
    void Stop();

    /**
     * @brief Check if worker is running
     */
    bool IsRunning() const { return running_; }

private:
    /**
     * @brief Worker thread function
     */
    void WorkerThreadFunc();

    /**
     * @brief Get frame interval based on sample rate
     * Each AAC frame = 1024 samples, so interval = 1024 / sample_rate seconds
     */
    std::chrono::microseconds GetFrameInterval() const;

    std::shared_ptr<lmshao::lmrtsp::RtspServerSession> session_;
    std::string file_path_;
    uint32_t sample_rate_;

    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> worker_thread_;
    std::unique_ptr<SessionAacReader> reader_;
};

#endif // SESSION_AAC_WORKER_THREAD_H
