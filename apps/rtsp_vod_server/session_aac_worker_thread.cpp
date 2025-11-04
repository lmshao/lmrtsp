/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_aac_worker_thread.h"

#include <lmcore/data_buffer.h>
#include <lmrtsp/media_types.h>

#include <chrono>
#include <iostream>

#include "file_manager.h"

SessionAacWorkerThread::SessionAacWorkerThread(std::shared_ptr<lmshao::lmrtsp::RtspServerSession> session,
                                               const std::string &file_path, uint32_t sample_rate)
    : session_(session), file_path_(file_path), sample_rate_(sample_rate)
{
}

SessionAacWorkerThread::~SessionAacWorkerThread()
{
    Stop();
}

bool SessionAacWorkerThread::Start()
{
    if (running_) {
        return false;
    }

    // Get mapped file from FileManager
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cerr << "Failed to get mapped file: " << file_path_ << std::endl;
        return false;
    }

    // Create reader
    reader_ = std::make_unique<SessionAacReader>(mapped_file);
    if (!reader_->IsValid()) {
        std::cerr << "Invalid AAC file: " << file_path_ << std::endl;
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
        return false;
    }

    // Use actual sample rate from file if available
    if (reader_->GetSampleRate() > 0) {
        sample_rate_ = reader_->GetSampleRate();
    }

    std::cout << "AAC worker thread starting for file: " << file_path_ << std::endl;
    std::cout << "  Sample rate: " << sample_rate_ << " Hz" << std::endl;
    std::cout << "  Channels: " << (int)reader_->GetChannels() << std::endl;
    std::cout << "  Bitrate: " << (reader_->GetBitrate() / 1000.0) << " kbps" << std::endl;

    running_ = true;
    worker_thread_ = std::make_unique<std::thread>(&SessionAacWorkerThread::WorkerThreadFunc, this);

    return true;
}

void SessionAacWorkerThread::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    // Release mapped file
    FileManager::GetInstance().ReleaseMappedFile(file_path_);
    reader_.reset();

    std::cout << "AAC worker thread stopped for file: " << file_path_ << std::endl;
}

std::chrono::microseconds SessionAacWorkerThread::GetFrameInterval() const
{
    // AAC-LC: 1024 samples per frame
    constexpr uint32_t SAMPLES_PER_FRAME = 1024;

    if (sample_rate_ == 0) {
        return std::chrono::microseconds(23219); // Default ~23.2ms for 44.1kHz
    }

    // Calculate interval: (samples / sample_rate) * 1000000 microseconds
    double interval_us = (double)SAMPLES_PER_FRAME / sample_rate_ * 1000000.0;
    return std::chrono::microseconds(static_cast<long long>(interval_us));
}

void SessionAacWorkerThread::WorkerThreadFunc()
{
    if (!reader_ || !session_) {
        return;
    }

    auto frame_interval = GetFrameInterval();
    std::cout << "AAC frame interval: " << frame_interval.count() / 1000.0 << " ms" << std::endl;

    auto playback_info = reader_->GetPlaybackInfo();
    std::cout << "Starting AAC playback: " << playback_info.total_frames_ << " frames, "
              << playback_info.total_duration_ << " seconds" << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    size_t frames_sent = 0;
    uint32_t timestamp = 0;

    while (running_) {
        std::vector<uint8_t> frame_data;

        if (!reader_->ReadNextFrame(frame_data)) {
            std::cout << "AAC playback finished (EOF)" << std::endl;
            break;
        }

        // Create MediaFrame
        lmshao::lmrtsp::MediaFrame frame;
        frame.media_type = lmshao::lmrtsp::MediaType::AAC;
        frame.timestamp = timestamp;
        frame.audio_param.sample_rate = sample_rate_;
        frame.audio_param.channels = reader_->GetChannels();

        // Copy frame data to DataBuffer
        frame.data = std::make_shared<lmshao::lmcore::DataBuffer>(frame_data.size());
        frame.data->Assign(frame_data.data(), frame_data.size());

        // Send frame to session using PushFrame
        if (!session_->PushFrame(frame)) {
            std::cerr << "Failed to push AAC frame, session may be closed" << std::endl;
            break;
        }

        frames_sent++;

        // Update timestamp (increment by samples per frame)
        timestamp += 1024;

        // Calculate next frame time
        auto expected_time = start_time + frame_interval * frames_sent;
        auto now = std::chrono::steady_clock::now();

        if (expected_time > now) {
            std::this_thread::sleep_until(expected_time);
        }

        // Check if we're falling behind
        if (frames_sent % 100 == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            if (elapsed.count() > 0) {
                std::cout << "AAC frames sent: " << frames_sent << " / " << playback_info.total_frames_ << " ("
                          << (frames_sent * 100 / playback_info.total_frames_) << "%)" << std::endl;
            }
        }
    }

    std::cout << "AAC worker thread completed. Frames sent: " << frames_sent << std::endl;
}
