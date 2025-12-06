/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_mkv_worker_thread.h"

#include <lmcore/data_buffer.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

#include "file_manager.h"
#include "session_mkv_reader.h"

SessionMkvWorkerThread::SessionMkvWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                                               uint64_t track_number, int rtsp_track_index, uint32_t frame_rate)
    : BaseSessionWorkerThread(session, file_path), track_number_(track_number), rtsp_track_index_(rtsp_track_index),
      frame_rate_(frame_rate), frame_counter_(0)
{
    if (!session_) {
        std::cout << "Invalid RtspServerSession provided to SessionMkvWorkerThread" << std::endl;
        return;
    }

    std::cout << "SessionMkvWorkerThread created for session: " << session_id_ << ", file: " << file_path_
              << ", track: " << track_number_ << ", frame_rate: " << frame_rate << std::endl;
}

SessionMkvWorkerThread::~SessionMkvWorkerThread()
{
    std::cout << "SessionMkvWorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionMkvWorkerThread::InitializeReader()
{
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    mkv_reader_ = std::make_unique<SessionMkvReader>(mapped_file, track_number_);

    if (!mkv_reader_->Initialize()) {
        std::cout << "Failed to initialize SessionMkvReader" << std::endl;
        mkv_reader_.reset();
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
        return false;
    }

    frame_counter_.store(0);
    return true;
}

void SessionMkvWorkerThread::CleanupReader()
{
    mkv_reader_.reset();
}

void SessionMkvWorkerThread::ReleaseFile()
{
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }
}

SessionMkvReader::PlaybackInfo SessionMkvWorkerThread::GetPlaybackInfo() const
{
    if (mkv_reader_) {
        return mkv_reader_->GetPlaybackInfo();
    }
    return SessionMkvReader::PlaybackInfo{};
}

void SessionMkvWorkerThread::Reset()
{
    ResetReader();
    frame_counter_.store(0);
    std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
}

void SessionMkvWorkerThread::ResetReader()
{
    if (mkv_reader_) {
        mkv_reader_->Reset();
    }
}

bool SessionMkvWorkerThread::SendNextData()
{
    return SendNextFrame();
}

std::chrono::microseconds SessionMkvWorkerThread::GetDataInterval() const
{
    uint32_t fps = frame_rate_.load();
    if (fps == 0) {
        fps = 25; // Default fallback
    }

    // Frame interval = (1 / fps) * 1,000,000 microseconds
    // Note: For audio, fps is scaled by 1000 for precision (e.g., 46875 means 46.875 fps)
    double interval_us;
    if (fps > 1000) {
        // Audio frame rate (scaled by 1000)
        interval_us = (1.0 / (fps / 1000.0)) * 1000000.0;
    } else {
        // Video frame rate (normal)
        interval_us = (1.0 / fps) * 1000000.0;
    }
    return std::chrono::microseconds(static_cast<long long>(interval_us));
}

bool SessionMkvWorkerThread::SendNextFrame()
{
    if (!mkv_reader_ || !session_) {
        std::cout << "Session " << session_id_ << " track " << rtsp_track_index_
                  << " SendNextFrame: reader or session is null" << std::endl;
        return false;
    }

    LocalMediaFrameMkv frame;
    if (!mkv_reader_->ReadNextFrame(frame)) {
        return false; // EOF or error
    }

    // Convert std::vector<uint8_t> to DataBuffer
    auto data_buffer = lmshao::lmcore::DataBuffer::Create(frame.data.size());
    data_buffer->Assign(frame.data.data(), frame.data.size());

    // Create MediaFrame for RTSP session
    lmshao::lmrtsp::MediaFrame rtsp_frame;
    rtsp_frame.data = data_buffer;
    rtsp_frame.timestamp = static_cast<uint32_t>(frame_counter_.load() * 3600); // 90kHz clock
    rtsp_frame.media_type = GetMediaType();

    // Send frame to session (multi-track version)
    bool success = session_->PushFrame(rtsp_frame, rtsp_track_index_);

    if (success) {
        data_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();
        frame_counter_++;

        // Only log every 100 frames to reduce output
        if (data_sent_.load() % 100 == 0) {
            std::cout << "Session " << session_id_ << " track " << rtsp_track_index_ << " sent " << data_sent_.load()
                      << " frames, " << bytes_sent_.load() << " bytes" << std::endl;
        }
    } else {
        // Log first failure
        if (data_sent_.load() == 0) {
            std::cout << "Session " << session_id_ << " track " << rtsp_track_index_ << " failed to send first frame"
                      << std::endl;
        }
    }

    return success;
}

MediaType SessionMkvWorkerThread::GetMediaType() const
{
    if (!mkv_reader_) {
        return MediaType::H264; // Default
    }

    std::string codec_id = mkv_reader_->GetCodecId();

    if (codec_id.find("V_MPEG4/ISO/AVC") == 0) {
        return MediaType::H264;
    } else if (codec_id.find("V_MPEGH/ISO/HEVC") == 0) {
        return MediaType::H265;
    } else if (codec_id.find("A_AAC") == 0) {
        return MediaType::AAC;
    }

    return MediaType::H264; // Default
}
