/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_session_worker_thread.h"

#include <iostream>

BaseSessionWorkerThread::BaseSessionWorkerThread(std::shared_ptr<RtspServerSession> session,
                                                 const std::string &file_path)
    : session_(session), file_path_(file_path), running_(false), should_stop_(false), data_sent_(0), bytes_sent_(0)
{
    if (session_) {
        session_id_ = session_->GetSessionId();
    }
}

BaseSessionWorkerThread::~BaseSessionWorkerThread()
{
    Stop();
}

bool BaseSessionWorkerThread::Start()
{
    if (running_.load()) {
        std::cout << "Worker thread already running for session: " << session_id_ << std::endl;
        return true;
    }

    if (!session_) {
        std::cout << "Cannot start worker thread: invalid session" << std::endl;
        return false;
    }

    // Initialize reader (implemented by derived class)
    if (!InitializeReader()) {
        std::cout << "Failed to initialize reader for session: " << session_id_ << std::endl;
        return false;
    }

    // Reset state
    should_stop_.store(false);
    data_sent_.store(0);
    bytes_sent_.store(0);
    start_time_ = std::chrono::steady_clock::now();
    last_data_time_ = start_time_;

    // Start worker thread
    worker_thread_ = std::make_unique<std::thread>(&BaseSessionWorkerThread::WorkerThreadFunc, this);

    running_.store(true);
    std::cout << "Worker thread started for session: " << session_id_ << std::endl;

    return true;
}

void BaseSessionWorkerThread::Stop()
{
    if (!running_.load()) {
        return;
    }

    std::cout << "Stopping worker thread for session: " << session_id_ << std::endl;

    should_stop_.store(true);

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    worker_thread_.reset();

    // Cleanup reader and file (implemented by derived class)
    CleanupReader();
    ReleaseFile();

    running_.store(false);

    std::cout << "Worker thread stopped for session: " << session_id_ << ", stats: " << data_sent_.load()
              << " data units, " << bytes_sent_.load() << " bytes" << std::endl;
}

void BaseSessionWorkerThread::WorkerThreadFunc()
{
    std::cout << "Worker thread started for session: " << session_id_ << std::endl;

    auto data_interval = GetDataInterval();

    while (!should_stop_.load()) {
        // Check if session is still active
        if (!IsSessionActive()) {
            std::cout << "Session " << session_id_ << " is no longer active, stopping worker" << std::endl;
            break;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_data_time_);

        // Send data if enough time has passed
        if (elapsed >= data_interval) {
            if (!SendNextData()) {
                // End of file or error
                HandleEOF();
                continue;
            }

            last_data_time_ = current_time;
            data_sent_++;
        }

        // Sleep for a short time to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    std::cout << "Worker thread finished for session: " << session_id_ << std::endl;
}

bool BaseSessionWorkerThread::IsSessionActive() const
{
    if (!session_) {
        return false;
    }

    // Check if session is still playing
    if (!session_->IsPlaying()) {
        return false;
    }

    // Check if network session is still valid
    auto network_session = session_->GetNetworkSession();
    if (!network_session) {
        return false;
    }

    return true;
}

void BaseSessionWorkerThread::HandleEOF()
{
    // Default behavior: loop back to beginning
    std::cout << "Session " << session_id_ << " reached EOF, looping back" << std::endl;
    ResetReader();
    start_time_ = std::chrono::steady_clock::now();
    last_data_time_ = start_time_;
    data_sent_.store(0);
}
