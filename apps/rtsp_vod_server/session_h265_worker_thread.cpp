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
#include <thread>

#include "session_h265_reader.h"

SessionH265WorkerThread::SessionH265WorkerThread(std::shared_ptr<RtspServerSession> session,
                                                 const std::string &file_path, uint32_t frame_rate)
    : session_(session), session_id_(session ? session->GetSessionId() : "unknown"), file_path_(file_path),
      running_(false), should_stop_(false), frame_rate_(frame_rate), frame_counter_(0), frames_sent_(0), bytes_sent_(0)
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
    Stop();
    std::cout << "SessionH265WorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionH265WorkerThread::Start()
{
    if (running_.load()) {
        std::cout << "SessionH265WorkerThread already running for session: " << session_id_ << std::endl;
        return true;
    }

    if (!session_) {
        std::cout << "Cannot start SessionH265WorkerThread: invalid session" << std::endl;
        return false;
    }

    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    h265_reader_ = std::make_unique<SessionH265Reader>(mapped_file);

    should_stop_.store(false);
    frame_counter_.store(0);
    frames_sent_.store(0);
    bytes_sent_.store(0);
    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;

    worker_thread_ = std::make_unique<std::thread>(&SessionH265WorkerThread::WorkerThreadFunc, this);

    running_.store(true);
    std::cout << "SessionH265WorkerThread started for session: " << session_id_ << std::endl;

    return true;
}

void SessionH265WorkerThread::Stop()
{
    if (!running_.load()) {
        return;
    }

    std::cout << "Stopping SessionH265WorkerThread for session: " << session_id_ << std::endl;

    should_stop_.store(true);

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    worker_thread_.reset();
    h265_reader_.reset();

    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }

    running_.store(false);

    std::cout << "SessionH265WorkerThread stopped for session: " << session_id_ << ", stats: " << frames_sent_.load()
              << " frames, " << bytes_sent_.load() << " bytes" << std::endl;
}

bool SessionH265WorkerThread::IsRunning() const
{
    return running_.load();
}

std::string SessionH265WorkerThread::GetSessionId() const
{
    return session_id_;
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
    if (h265_reader_) {
        h265_reader_->Reset();
        frame_counter_.store(0);
        start_time_ = std::chrono::steady_clock::now();
        last_frame_time_ = start_time_;
        std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
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

void SessionH265WorkerThread::WorkerThreadFunc()
{
    std::cout << "Worker thread started for session: " << session_id_ << std::endl;

    auto frame_interval = GetFrameInterval();

    while (!should_stop_.load()) {
        if (!IsSessionActive()) {
            std::cout << "Session " << session_id_ << " is no longer active, stopping worker" << std::endl;
            break;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_frame_time_);

        if (elapsed >= frame_interval) {
            if (!SendNextFrame()) {
                std::cout << "Session " << session_id_ << " reached EOF, looping back" << std::endl;
                Reset();
                continue;
            }

            last_frame_time_ = current_time;
            frame_counter_++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "Worker thread finished for session: " << session_id_ << std::endl;
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
    rtsp_frame.timestamp = static_cast<uint32_t>(frame.timestamp);
    rtsp_frame.media_type = MediaType::H265;
    rtsp_frame.video_param.is_key_frame = frame.is_keyframe;

    bool success = session_->PushFrame(rtsp_frame);

    if (success) {
        frames_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();

        std::cout << "Session " << session_id_ << " sent frame " << frames_sent_.load()
                  << ", size: " << rtsp_frame.data->Size() << " bytes, timestamp: " << rtsp_frame.timestamp
                  << ", keyframe: " << rtsp_frame.video_param.is_key_frame << std::endl;
    } else {
        std::cout << "Session " << session_id_ << " failed to send frame" << std::endl;
    }

    return success;
}

std::chrono::milliseconds SessionH265WorkerThread::GetFrameInterval() const
{
    uint32_t fps = frame_rate_.load();
    if (fps == 0) {
        fps = 25;
    }
    return std::chrono::milliseconds(1000 / fps);
}

bool SessionH265WorkerThread::IsSessionActive() const
{
    if (!session_) {
        return false;
    }

    if (!session_->IsPlaying()) {
        return false;
    }

    auto network_session = session_->GetNetworkSession();
    if (!network_session) {
        return false;
    }

    return true;
}
