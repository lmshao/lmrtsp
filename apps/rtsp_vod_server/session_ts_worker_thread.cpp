/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_ts_worker_thread.h"

#include <lmcore/data_buffer.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

SessionTSWorkerThread::SessionTSWorkerThread(std::shared_ptr<RtspSession> session, const std::string &file_path,
                                             uint32_t bitrate)
    : session_(session), session_id_(session ? session->GetSessionId() : "unknown"), file_path_(file_path),
      running_(false), should_stop_(false), bitrate_(bitrate), packet_counter_(0), packets_sent_(0), bytes_sent_(0)
{
    if (!session_) {
        std::cout << "Invalid RtspSession provided to SessionTSWorkerThread" << std::endl;
        return;
    }

    std::cout << "SessionTSWorkerThread created for session: " << session_id_ << ", file: " << file_path_
              << ", bitrate: " << (bitrate / 1000000.0) << " Mbps" << std::endl;
}

SessionTSWorkerThread::~SessionTSWorkerThread()
{
    Stop();
    std::cout << "SessionTSWorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionTSWorkerThread::Start()
{
    if (running_.load()) {
        std::cout << "SessionTSWorkerThread already running for session: " << session_id_ << std::endl;
        return true;
    }

    if (!session_) {
        std::cout << "Cannot start SessionTSWorkerThread: invalid session" << std::endl;
        return false;
    }

    // Get shared MappedFile through FileManager
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    // Create SessionTSReader for independent playback
    ts_reader_ = std::make_unique<SessionTSReader>(mapped_file);

    // Reset state
    should_stop_.store(false);
    packet_counter_.store(0);
    packets_sent_.store(0);
    bytes_sent_.store(0);
    start_time_ = std::chrono::steady_clock::now();
    last_packet_time_ = start_time_;

    // Start worker thread
    worker_thread_ = std::make_unique<std::thread>(&SessionTSWorkerThread::WorkerThreadFunc, this);

    running_.store(true);
    std::cout << "SessionTSWorkerThread started for session: " << session_id_ << std::endl;

    return true;
}

void SessionTSWorkerThread::Stop()
{
    if (!running_.load()) {
        return;
    }

    std::cout << "Stopping SessionTSWorkerThread for session: " << session_id_ << std::endl;

    should_stop_.store(true);

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    worker_thread_.reset();
    ts_reader_.reset();

    // Release MappedFile reference
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }

    running_.store(false);

    std::cout << "SessionTSWorkerThread stopped for session: " << session_id_ << ", stats: " << packets_sent_.load()
              << " packets, " << bytes_sent_.load() << " bytes" << std::endl;
}

bool SessionTSWorkerThread::IsRunning() const
{
    return running_.load();
}

std::string SessionTSWorkerThread::GetSessionId() const
{
    return session_id_;
}

SessionTSReader::PlaybackInfo SessionTSWorkerThread::GetPlaybackInfo() const
{
    if (ts_reader_) {
        return ts_reader_->GetPlaybackInfo();
    }
    return SessionTSReader::PlaybackInfo{};
}

void SessionTSWorkerThread::Reset()
{
    if (ts_reader_) {
        ts_reader_->Reset();
        packet_counter_.store(0);
        start_time_ = std::chrono::steady_clock::now();
        last_packet_time_ = start_time_;
        std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
    }
}

// Private methods implementation

void SessionTSWorkerThread::WorkerThreadFunc()
{
    std::cout << "TS Worker thread started for session: " << session_id_ << std::endl;

    auto packet_interval = GetPacketInterval();

    while (!should_stop_.load()) {
        // Check if session is still active
        if (!IsSessionActive()) {
            std::cout << "Session " << session_id_ << " is no longer active, stopping worker" << std::endl;
            break;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_packet_time_);

        // Send packet if enough time has passed
        if (elapsed >= packet_interval) {
            if (!SendNextPacket()) {
                // End of file or error, loop back to beginning
                std::cout << "Session " << session_id_ << " reached EOF, looping back" << std::endl;
                Reset();
                continue;
            }

            last_packet_time_ = current_time;
            packet_counter_++;
        }

        // Sleep for a short time to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    std::cout << "TS Worker thread finished for session: " << session_id_ << std::endl;
}

bool SessionTSWorkerThread::SendNextPacket()
{
    if (!ts_reader_ || !session_) {
        return false;
    }

    std::vector<uint8_t> packet_data;
    if (!ts_reader_->ReadNextPacket(packet_data)) {
        return false; // EOF or error
    }

    // Convert std::vector<uint8_t> to DataBuffer
    auto data_buffer = lmshao::lmcore::DataBuffer::Create(packet_data.size());
    data_buffer->Assign(packet_data.data(), packet_data.size());

    // Create MediaFrame for RTSP session
    lmshao::lmrtsp::MediaFrame rtsp_frame;
    rtsp_frame.data = data_buffer;
    rtsp_frame.timestamp = static_cast<uint32_t>(packet_counter_.load() * 3600); // 90kHz clock, ~40ms per packet
    rtsp_frame.media_type = MediaType::MP2T;

    // Send frame to session
    bool success = session_->PushFrame(rtsp_frame);

    if (success) {
        packets_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();

        // Only log every 50 packets to reduce output
        if (packets_sent_.load() % 50 == 0) {
            std::cout << "Session " << session_id_ << " sent " << packets_sent_.load() << " packets, "
                      << bytes_sent_.load() << " bytes" << std::endl;
        }
    } else {
        std::cout << "Session " << session_id_ << " failed to send packet" << std::endl;
    }

    return success;
}

std::chrono::microseconds SessionTSWorkerThread::GetPacketInterval() const
{
    constexpr size_t TS_PACKET_SIZE = 188; // Standard TS packet size

    uint32_t bps = bitrate_.load();
    if (bps == 0) {
        bps = 2000000; // 2 Mbps default fallback
    }

    // TS packet = 188 bytes = 1504 bits
    // Interval = (bits_per_packet / bits_per_second) * 1,000,000 microseconds
    double interval_us = (TS_PACKET_SIZE * 8.0 / bps) * 1000000.0;
    return std::chrono::microseconds(static_cast<long long>(interval_us));
}

bool SessionTSWorkerThread::IsSessionActive() const
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
