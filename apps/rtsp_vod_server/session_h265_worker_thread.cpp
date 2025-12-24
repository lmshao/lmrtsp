/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_h265_worker_thread.h"

#include <lmcore/data_buffer.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

#include "file_manager.h"
#include "session_h265_reader.h"

SessionH265WorkerThread::SessionH265WorkerThread(std::shared_ptr<RtspServerSession> session,
                                                 const std::string &file_path, uint32_t frame_rate)
    : BaseSessionWorkerThread(session, file_path), frame_rate_(frame_rate), frame_counter_(0),
      rtp_timestamp_increment_(3600)
{
    if (!session_) {
        std::cout << "Invalid RtspServerSession provided to SessionH265WorkerThread" << std::endl;
        return;
    }

    std::cout << "SessionH265WorkerThread created for session: " << session_id_ << ", file: " << file_path_
              << ", fps: " << frame_rate << std::endl;
}

SessionH265WorkerThread::~SessionH265WorkerThread()
{
    std::cout << "SessionH265WorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionH265WorkerThread::InitializeReader()
{
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    h265_reader_ = std::make_unique<SessionH265Reader>(mapped_file);
    frame_counter_.store(0);

    // Calculate RTP timestamp increment based on frame rate
    // RTP clock is 90kHz, so increment = 90000 / fps
    uint32_t fps = frame_rate_.load();
    if (fps == 0) {
        fps = 25; // Default fallback
    }
    rtp_timestamp_increment_ = 90000 / fps;

    std::cout << "RTP timestamp increment: " << rtp_timestamp_increment_ << " (90kHz clock, fps=" << fps << ")"
              << std::endl;

    return true;
}

void SessionH265WorkerThread::CleanupReader()
{
    h265_reader_.reset();
}

void SessionH265WorkerThread::ReleaseFile()
{
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }
}

SessionH265Reader::PlaybackInfo SessionH265WorkerThread::GetPlaybackInfo() const
{
    if (h265_reader_) {
        return h265_reader_->GetPlaybackInfo();
    }
    return SessionH265Reader::PlaybackInfo{};
}

bool SessionH265WorkerThread::SeekToFrame(size_t frame_index)
{
    if (h265_reader_) {
        bool result = h265_reader_->SeekToFrame(frame_index);
        if (result) {
            std::cout << "Session " << session_id_ << " seeked to frame: " << frame_index << std::endl;
        }
        return result;
    }
    return false;
}

bool SessionH265WorkerThread::SeekToTime(double timestamp)
{
    if (h265_reader_) {
        bool result = h265_reader_->SeekToTime(timestamp);
        if (result) {
            std::cout << "Session " << session_id_ << " seeked to time: " << std::fixed << std::setprecision(2)
                      << timestamp << "s" << std::endl;
        }
        return result;
    }
    return false;
}

void SessionH265WorkerThread::Reset()
{
    ResetReader();
    frame_counter_.store(0);
    std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
}

void SessionH265WorkerThread::ResetReader()
{
    if (h265_reader_) {
        h265_reader_->Reset();
    }
}

void SessionH265WorkerThread::SetFrameRate(uint32_t fps)
{
    if (fps > 0 && fps <= 120) {
        frame_rate_.store(fps);
        std::cout << "Session " << session_id_ << " frame rate set to: " << fps << " fps" << std::endl;
    } else {
        std::cout << "Invalid frame rate: " << fps << ", keeping current: " << frame_rate_.load() << std::endl;
    }
}

uint32_t SessionH265WorkerThread::GetFrameRate() const
{
    return frame_rate_.load();
}

bool SessionH265WorkerThread::SendNextData()
{
    return SendNextFrame();
}

std::chrono::microseconds SessionH265WorkerThread::GetDataInterval() const
{
    uint32_t fps = frame_rate_.load();
    if (fps == 0) {
        fps = 25;
    }
    return std::chrono::microseconds(1000000 / fps);
}

bool SessionH265WorkerThread::SendNextFrame()
{
    if (!h265_reader_ || !session_) {
        return false;
    }

    LocalMediaFrameH265 frame;
    if (!h265_reader_->ReadNextFrame(frame)) {
        return false;
    }

    auto data_buffer = lmshao::lmcore::DataBuffer::Create(frame.data.size());
    data_buffer->Assign(frame.data.data(), frame.data.size());

    lmshao::lmrtsp::MediaFrame rtsp_frame;
    rtsp_frame.data = data_buffer;
    // Calculate RTP timestamp using frame counter and increment
    // RTP timestamp must be in 90kHz clock units for proper playback synchronization
    // Using frame_counter ensures continuous, monotonic timestamps that VLC requires
    rtsp_frame.timestamp = static_cast<uint32_t>(frame_counter_.load() * rtp_timestamp_increment_);
    rtsp_frame.media_type = MediaType::H265;
    rtsp_frame.video_param.is_key_frame = frame.is_keyframe;

    bool success = session_->PushFrame(rtsp_frame);

    if (success) {
        data_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();
        frame_counter_++;

        std::cout << "Session " << session_id_ << " sent frame " << data_sent_.load()
                  << ", size: " << rtsp_frame.data->Size() << " bytes, RTP timestamp: " << rtsp_frame.timestamp
                  << ", keyframe: " << rtsp_frame.video_param.is_key_frame << std::endl;
    } else {
        std::cout << "Session " << session_id_ << " failed to send frame" << std::endl;
    }

    return success;
}
