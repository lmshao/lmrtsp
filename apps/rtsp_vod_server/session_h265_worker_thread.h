/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_H265_WORKER_THREAD_H
#define LMSHAO_RTSP_SESSION_H265_WORKER_THREAD_H

#include <lmcore/data_buffer.h>
#include <lmrtsp/media_types.h>
#include <lmrtsp/rtsp_server_session.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "file_manager.h"
#include "session_h265_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling H.265 RTSP client session
 */
class SessionH265WorkerThread {
public:
    SessionH265WorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                            uint32_t frame_rate = 25);
    ~SessionH265WorkerThread();

    bool Start();
    void Stop();
    bool IsRunning() const;
    std::string GetSessionId() const;
    SessionH265Reader::PlaybackInfo GetPlaybackInfo() const;
    bool SeekToFrame(size_t frame_index);
    bool SeekToTime(double timestamp);
    void Reset();
    void SetFrameRate(uint32_t fps);
    uint32_t GetFrameRate() const;

private:
    void WorkerThreadFunc();
    bool SendNextFrame();
    std::chrono::milliseconds GetFrameInterval() const;
    bool IsSessionActive() const;

private:
    std::shared_ptr<RtspServerSession> session_;
    std::string session_id_;
    std::string file_path_;
    std::unique_ptr<SessionH265Reader> h265_reader_;
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    std::atomic<uint32_t> frame_rate_;
    std::atomic<uint64_t> frame_counter_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
    std::atomic<size_t> frames_sent_;
    std::atomic<size_t> bytes_sent_;
};

#endif // LMSHAO_RTSP_SESSION_H265_WORKER_THREAD_H
