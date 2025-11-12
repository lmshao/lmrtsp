/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_MKV_WORKER_THREAD_H
#define LMSHAO_RTSP_SESSION_MKV_WORKER_THREAD_H

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
#include "session_mkv_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling MKV streaming
 */
class SessionMkvWorkerThread : public BaseSessionWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to handle
     * @param file_path Path to the MKV file
     * @param track_number Target track number
     * @param frame_rate Target frame rate for streaming (video) or sample rate (audio)
     */
    SessionMkvWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                           uint64_t track_number, int rtsp_track_index, uint32_t frame_rate = 25);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~SessionMkvWorkerThread();

    /**
     * @brief Get current playback information
     * @return Playback info structure
     */
    SessionMkvReader::PlaybackInfo GetPlaybackInfo() const;

    /**
     * @brief Reset playback to beginning
     */
    void Reset() override;

protected:
    bool InitializeReader() override;
    bool SendNextData() override;
    std::chrono::microseconds GetDataInterval() const override;
    void ResetReader() override;
    void CleanupReader() override;
    void ReleaseFile() override;

private:
    /**
     * @brief Send next frame to client (internal implementation)
     * @return true if frame sent successfully, false if EOF or error
     */
    bool SendNextFrame();

    /**
     * @brief Determine media type from codec ID
     */
    MediaType GetMediaType() const;

    uint64_t track_number_;
    int rtsp_track_index_; // RTSP track index (0, 1, ...) for PushFrame
    std::unique_ptr<SessionMkvReader> mkv_reader_;
    std::atomic<uint32_t> frame_rate_; // frames per second (video) or samples per second (audio)
    std::atomic<uint64_t> frame_counter_;
};

#endif // LMSHAO_RTSP_SESSION_MKV_WORKER_THREAD_H
