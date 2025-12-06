/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_h264_worker_thread.h"

#include <lmcore/data_buffer.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

#include "file_manager.h"
#include "session_h264_reader.h"

SessionH264WorkerThread::SessionH264WorkerThread(std::shared_ptr<RtspServerSession> session,
                                                 const std::string &file_path, uint32_t frame_rate)
    : BaseSessionWorkerThread(session, file_path), frame_rate_(frame_rate), frame_counter_(0)
{
    if (!session_) {
        std::cout << "Invalid RtspServerSession provided to SessionH264WorkerThread" << std::endl;
        return;
    }

    std::cout << "SessionH264WorkerThread created for session: " << session_id_ << ", file: " << file_path_
              << ", fps: " << frame_rate << std::endl;
}

SessionH264WorkerThread::~SessionH264WorkerThread()
{
    std::cout << "SessionH264WorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionH264WorkerThread::InitializeReader()
{
    // Get shared MappedFile through FileManager
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    // Create SessionH264Reader for independent playback
    h264_reader_ = std::make_unique<SessionH264Reader>(mapped_file);
    frame_counter_.store(0);

    return true;
}

void SessionH264WorkerThread::CleanupReader()
{
    h264_reader_.reset();
}

void SessionH264WorkerThread::ReleaseFile()
{
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }
}

SessionH264Reader::PlaybackInfo SessionH264WorkerThread::GetPlaybackInfo() const
{
    if (h264_reader_) {
        return h264_reader_->GetPlaybackInfo();
    }
    return SessionH264Reader::PlaybackInfo{};
}

bool SessionH264WorkerThread::SeekToFrame(size_t frame_index)
{
    if (h264_reader_) {
        bool result = h264_reader_->SeekToFrame(frame_index);
        if (result) {
            std::cout << "Session " << session_id_ << " seeked to frame: " << frame_index << std::endl;
        }
        return result;
    }
    return false;
}

bool SessionH264WorkerThread::SeekToTime(double timestamp)
{
    if (h264_reader_) {
        bool result = h264_reader_->SeekToTime(timestamp);
        if (result) {
            std::cout << "Session " << session_id_ << " seeked to time: " << std::fixed << std::setprecision(2)
                      << timestamp << "s" << std::endl;
        }
        return result;
    }
    return false;
}

void SessionH264WorkerThread::Reset()
{
    ResetReader();
    frame_counter_.store(0);
    std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
}

void SessionH264WorkerThread::ResetReader()
{
    if (h264_reader_) {
        h264_reader_->Reset();
    }
}

void SessionH264WorkerThread::SetFrameRate(uint32_t fps)
{
    if (fps > 0 && fps <= 120) { // Reasonable range
        frame_rate_.store(fps);
        std::cout << "Session " << session_id_ << " frame rate set to: " << fps << " fps" << std::endl;
    } else {
        std::cout << "Invalid frame rate: " << fps << ", keeping current: " << frame_rate_.load() << std::endl;
    }
}

uint32_t SessionH264WorkerThread::GetFrameRate() const
{
    return frame_rate_.load();
}

// Protected methods implementation

bool SessionH264WorkerThread::SendNextData()
{
    return SendNextFrame();
}

std::chrono::microseconds SessionH264WorkerThread::GetDataInterval() const
{
    uint32_t fps = frame_rate_.load();
    if (fps == 0) {
        fps = 25; // Default fallback
    }
    // Convert milliseconds to microseconds
    return std::chrono::microseconds(1000000 / fps);
}

// Private methods implementation

bool SessionH264WorkerThread::SendNextFrame()
{
    if (!h264_reader_ || !session_) {
        return false;
    }

    LocalMediaFrame frame;
    if (!h264_reader_->ReadNextFrame(frame)) {
        return false; // EOF or error
    }

    // Convert std::vector<uint8_t> to DataBuffer
    auto data_buffer = lmshao::lmcore::DataBuffer::Create(frame.data.size());
    data_buffer->Assign(frame.data.data(), frame.data.size());

    // Create MediaFrame for RTSP session
    lmshao::lmrtsp::MediaFrame rtsp_frame;
    rtsp_frame.data = data_buffer;
    rtsp_frame.timestamp = static_cast<uint32_t>(frame.timestamp);
    rtsp_frame.media_type = MediaType::H264;
    rtsp_frame.video_param.is_key_frame = frame.is_keyframe;

    // Send frame to session
    bool success = session_->PushFrame(rtsp_frame);

    if (success) {
        data_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();
        frame_counter_++;

        std::cout << "Session " << session_id_ << " sent frame " << data_sent_.load()
                  << ", size: " << rtsp_frame.data->Size() << " bytes, timestamp: " << rtsp_frame.timestamp
                  << ", keyframe: " << rtsp_frame.video_param.is_key_frame << std::endl;
    } else {
        std::cout << "Session " << session_id_ << " failed to send frame" << std::endl;
    }

    return success;
}