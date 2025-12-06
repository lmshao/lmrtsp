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

#include "file_manager.h"
#include "session_ts_reader.h"

SessionTSWorkerThread::SessionTSWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                                             uint32_t bitrate)
    : BaseSessionWorkerThread(session, file_path), bitrate_(bitrate), packet_counter_(0)
{
    if (!session_) {
        std::cout << "Invalid RtspServerSession provided to SessionTSWorkerThread" << std::endl;
        return;
    }

    std::cout << "SessionTSWorkerThread created for session: " << session_id_ << ", file: " << file_path_
              << ", bitrate: " << (bitrate / 1000000.0) << " Mbps" << std::endl;
}

SessionTSWorkerThread::~SessionTSWorkerThread()
{
    std::cout << "SessionTSWorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionTSWorkerThread::InitializeReader()
{
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    ts_reader_ = std::make_unique<SessionTSReader>(mapped_file);
    packet_counter_.store(0);

    return true;
}

void SessionTSWorkerThread::CleanupReader()
{
    ts_reader_.reset();
}

void SessionTSWorkerThread::ReleaseFile()
{
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }
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
    ResetReader();
    packet_counter_.store(0);
    std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
}

void SessionTSWorkerThread::ResetReader()
{
    if (ts_reader_) {
        ts_reader_->Reset();
    }
}

bool SessionTSWorkerThread::SendNextData()
{
    return SendNextPacket();
}

std::chrono::microseconds SessionTSWorkerThread::GetDataInterval() const
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
        data_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();
        packet_counter_++;

        // Only log every 50 packets to reduce output
        if (data_sent_.load() % 50 == 0) {
            std::cout << "Session " << session_id_ << " sent " << data_sent_.load() << " packets, "
                      << bytes_sent_.load() << " bytes" << std::endl;
        }
    } else {
        std::cout << "Session " << session_id_ << " failed to send packet" << std::endl;
    }

    return success;
}
