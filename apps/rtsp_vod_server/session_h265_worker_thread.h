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

#include "base_session_worker_thread.h"
#include "file_manager.h"
#include "session_h265_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling H.265 RTSP client session
 */
class SessionH265WorkerThread : public BaseSessionWorkerThread {
public:
    SessionH265WorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                            uint32_t frame_rate = 25);
    ~SessionH265WorkerThread();
    SessionH265Reader::PlaybackInfo GetPlaybackInfo() const;
    bool SeekToFrame(size_t frame_index);
    bool SeekToTime(double timestamp);
    void Reset() override;
    void SetFrameRate(uint32_t fps);
    uint32_t GetFrameRate() const;

protected:
    bool InitializeReader() override;
    bool SendNextData() override;
    std::chrono::microseconds GetDataInterval() const override;
    void ResetReader() override;
    void CleanupReader() override;
    void ReleaseFile() override;

private:
    bool SendNextFrame();
    std::unique_ptr<SessionH265Reader> h265_reader_;
    std::atomic<uint32_t> frame_rate_;
    std::atomic<uint64_t> frame_counter_;
};

#endif // LMSHAO_RTSP_SESSION_H265_WORKER_THREAD_H
