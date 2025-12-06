/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_TS_WORKER_THREAD_H
#define LMSHAO_RTSP_SESSION_TS_WORKER_THREAD_H

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
#include "session_ts_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling TS (MPEG-TS) streaming
 */
class SessionTSWorkerThread : public BaseSessionWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to handle
     * @param file_path Path to the TS file
     * @param bitrate Target bitrate for streaming (bps)
     */
    SessionTSWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                          uint32_t bitrate = 2000000);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~SessionTSWorkerThread();

    /**
     * @brief Get current playback information
     * @return Playback info structure
     */
    SessionTSReader::PlaybackInfo GetPlaybackInfo() const;

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
     * @brief Send next TS packet to client (internal implementation)
     * @return true if packet sent successfully, false if EOF or error
     */
    bool SendNextPacket();

    // TS reader for independent playback
    std::unique_ptr<SessionTSReader> ts_reader_;

    // Streaming parameters
    std::atomic<uint32_t> bitrate_; // bits per second
    std::atomic<uint64_t> packet_counter_;
};

#endif // LMSHAO_RTSP_SESSION_TS_WORKER_THREAD_H
