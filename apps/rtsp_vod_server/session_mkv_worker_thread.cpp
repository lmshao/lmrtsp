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
#include <thread>

SessionMkvWorkerThread::SessionMkvWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                                               uint64_t track_number, int rtsp_track_index, uint32_t frame_rate)
    : session_(session), session_id_(session ? session->GetSessionId() : "unknown"), file_path_(file_path),
      track_number_(track_number), rtsp_track_index_(rtsp_track_index), running_(false), should_stop_(false),
      frame_rate_(frame_rate), frame_counter_(0), frames_sent_(0), bytes_sent_(0)
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
    Stop();
    std::cout << "SessionMkvWorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionMkvWorkerThread::Start()
{
    if (running_.load()) {
        std::cout << "SessionMkvWorkerThread already running for session: " << session_id_ << std::endl;
        return true;
    }

    if (!session_) {
        std::cout << "Cannot start SessionMkvWorkerThread: invalid session" << std::endl;
        return false;
    }

    // Get shared MappedFile through FileManager
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    // Create SessionMkvReader for independent playback
    mkv_reader_ = std::make_unique<SessionMkvReader>(mapped_file, track_number_);

    if (!mkv_reader_->Initialize()) {
        std::cout << "Failed to initialize SessionMkvReader" << std::endl;
        mkv_reader_.reset();
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
        return false;
    }

    // Note: frame_rate_ is already set in constructor based on media type (video fps or audio sample rate)
    // Don't override it here to preserve the correct rate for audio tracks

    // Reset state
    should_stop_.store(false);
    frame_counter_.store(0);
    frames_sent_.store(0);
    bytes_sent_.store(0);
    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;

    // Start worker thread
    worker_thread_ = std::make_unique<std::thread>(&SessionMkvWorkerThread::WorkerThreadFunc, this);

    running_.store(true);
    std::cout << "SessionMkvWorkerThread started for session: " << session_id_ << std::endl;

    return true;
}

void SessionMkvWorkerThread::Stop()
{
    if (!running_.load()) {
        return;
    }

    std::cout << "Stopping SessionMkvWorkerThread for session: " << session_id_ << std::endl;

    should_stop_.store(true);

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    worker_thread_.reset();
    mkv_reader_.reset();

    // Release MappedFile reference
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }

    running_.store(false);

    std::cout << "SessionMkvWorkerThread stopped for session: " << session_id_ << ", stats: " << frames_sent_.load()
              << " frames, " << bytes_sent_.load() << " bytes" << std::endl;
}

bool SessionMkvWorkerThread::IsRunning() const
{
    return running_.load();
}

std::string SessionMkvWorkerThread::GetSessionId() const
{
    return session_id_;
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
    if (mkv_reader_) {
        mkv_reader_->Reset();
        frame_counter_.store(0);
        start_time_ = std::chrono::steady_clock::now();
        last_frame_time_ = start_time_;
        std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
    }
}

// Private methods implementation

void SessionMkvWorkerThread::WorkerThreadFunc()
{
    std::cout << "[Worker-" << rtsp_track_index_ << "] Thread started, session: " << session_id_ << std::endl;
    std::cout << "[Worker-" << rtsp_track_index_ << "] frame_rate_=" << frame_rate_.load() << std::endl;

    auto frame_interval = GetFrameInterval();
    std::cout << "[Worker-" << rtsp_track_index_
              << "] Frame interval: " << std::chrono::duration_cast<std::chrono::milliseconds>(frame_interval).count()
              << "ms" << std::endl;

    // Reset last_frame_time_ to NOW to avoid burst sending due to thread startup delay
    last_frame_time_ = std::chrono::steady_clock::now();

    while (!should_stop_.load()) {
        // Check if session is still active
        if (!IsSessionActive()) {
            std::cout << "Session " << session_id_ << " is no longer active, stopping worker" << std::endl;
            break;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time_);

        // Send frame if enough time has passed
        if (elapsed >= frame_interval) {
            bool send_result = SendNextFrame();
            if (!send_result) {
                // End of file, loop back to beginning
                std::cout << "Session " << session_id_ << " track " << rtsp_track_index_ << " reached EOF, looping back"
                          << std::endl;
                Reset();
                // Continue after reset, but still update time to avoid rapid looping
            }

            // Update time by adding frame_interval (not current_time) to maintain stable rate
            // This prevents "catch-up" bursts when there's occasional delay
            last_frame_time_ += frame_interval;

            // If we're too far behind (> 5 frames), reset to avoid burst
            if (elapsed > frame_interval * 5) {
                last_frame_time_ = current_time;
            }

            frame_counter_++;

            // Log every 100 frames
            if (frame_counter_.load() % 100 == 0) {
                std::cout << "Session " << session_id_ << " track " << rtsp_track_index_ << ": sent "
                          << frame_counter_.load() << " frames" << std::endl;
            }
        }

        // Calculate how long to sleep based on remaining time until next frame
        auto time_until_next_frame = frame_interval - elapsed;
        if (time_until_next_frame > std::chrono::microseconds(1000)) {
            // Sleep for most of the remaining time (留1ms余量避免过冲)
            std::this_thread::sleep_for(time_until_next_frame - std::chrono::microseconds(1000));
        } else {
            // Sleep for a minimal time to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    std::cout << "MKV Worker thread finished for session: " << session_id_ << std::endl;
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
        frames_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();

        // Only log every 100 frames to reduce output
        if (frames_sent_.load() % 100 == 0) {
            std::cout << "Session " << session_id_ << " track " << rtsp_track_index_ << " sent " << frames_sent_.load()
                      << " frames, " << bytes_sent_.load() << " bytes" << std::endl;
        }
    } else {
        // Log first failure
        if (frames_sent_.load() == 0) {
            std::cout << "Session " << session_id_ << " track " << rtsp_track_index_ << " failed to send first frame"
                      << std::endl;
        }
    }

    return success;
}

std::chrono::microseconds SessionMkvWorkerThread::GetFrameInterval() const
{
    uint32_t fps = frame_rate_.load();
    std::cout << "[GetInterval-" << rtsp_track_index_ << "] fps read from frame_rate_: " << fps << std::endl;

    if (fps == 0) {
        fps = 25; // Default fallback
        std::cout << "[GetInterval-" << rtsp_track_index_ << "] fps was 0, using default 25" << std::endl;
    }

    // Frame interval = (1 / fps) * 1,000,000 microseconds
    // Note: For audio, fps is scaled by 1000 for precision (e.g., 46875 means 46.875 fps)
    double interval_us;
    if (fps > 1000) {
        // Audio frame rate (scaled by 1000)
        interval_us = (1.0 / (fps / 1000.0)) * 1000000.0;
        std::cout << "[GetInterval-" << rtsp_track_index_ << "] Audio: fps=" << fps << ", interval_us=" << interval_us
                  << std::endl;
    } else {
        // Video frame rate (normal)
        interval_us = (1.0 / fps) * 1000000.0;
        std::cout << "[GetInterval-" << rtsp_track_index_ << "] Video: fps=" << fps << ", interval_us=" << interval_us
                  << std::endl;
    }
    return std::chrono::microseconds(static_cast<long long>(interval_us));
}

bool SessionMkvWorkerThread::IsSessionActive() const
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
