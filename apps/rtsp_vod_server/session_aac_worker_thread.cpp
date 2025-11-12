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
#include "session_aac_reader.h"

SessionAacWorkerThread::SessionAacWorkerThread(std::shared_ptr<lmshao::lmrtsp::RtspServerSession> session,
                                               const std::string &file_path, uint32_t sample_rate)
    : BaseSessionWorkerThread(session, file_path), sample_rate_(sample_rate)
{
}

SessionAacWorkerThread::~SessionAacWorkerThread() {}

bool SessionAacWorkerThread::InitializeReader()
{
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cerr << "Failed to get mapped file: " << file_path_ << std::endl;
        return false;
    }

    reader_ = std::make_unique<SessionAacReader>(mapped_file);
    if (!reader_->IsValid()) {
        std::cerr << "Invalid AAC file: " << file_path_ << std::endl;
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
        reader_.reset();
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

    return true;
}

void SessionAacWorkerThread::CleanupReader()
{
    reader_.reset();
}

void SessionAacWorkerThread::ReleaseFile()
{
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }
}

void SessionAacWorkerThread::Reset()
{
    ResetReader();
}

void SessionAacWorkerThread::ResetReader()
{
    if (reader_) {
        reader_->Reset();
    }
}

bool SessionAacWorkerThread::SendNextData()
{
    if (!reader_ || !session_) {
        return false;
    }

    std::vector<uint8_t> frame_data;
    if (!reader_->ReadNextFrame(frame_data)) {
        return false; // EOF or error
    }

    // Create MediaFrame
    lmshao::lmrtsp::MediaFrame frame;
    frame.media_type = lmshao::lmrtsp::MediaType::AAC;
    frame.timestamp = static_cast<uint32_t>(data_sent_.load() * 1024); // Increment by samples per frame
    frame.audio_param.sample_rate = sample_rate_;
    frame.audio_param.channels = reader_->GetChannels();

    // Copy frame data to DataBuffer
    frame.data = lmshao::lmcore::DataBuffer::Create(frame_data.size());
    frame.data->Assign(frame_data.data(), frame_data.size());

    // Send frame to session using PushFrame
    bool success = session_->PushFrame(frame);

    if (success) {
        bytes_sent_ += frame.data->Size();
        // Log every 100 frames
        if (data_sent_.load() % 100 == 0) {
            std::cout << "AAC frames sent: " << data_sent_.load() << std::endl;
        }
    } else {
        std::cerr << "Failed to push AAC frame, session may be closed" << std::endl;
    }

    return success;
}

std::chrono::microseconds SessionAacWorkerThread::GetDataInterval() const
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
