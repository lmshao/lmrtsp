/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SESSION_AAC_WORKER_THREAD_H
#define SESSION_AAC_WORKER_THREAD_H

#include <lmrtsp/rtsp_server_session.h>

#include <atomic>
#include <memory>
#include <string>

#include "base_session_worker_thread.h"
#include "session_aac_reader.h"

/**
 * @brief Worker thread for AAC audio streaming
 *
 * Reads AAC frames from file and sends them via RTP at correct timing.
 * Uses frame duration (1024 samples per frame) for timing control.
 */
class SessionAacWorkerThread : public BaseSessionWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to send data to
     * @param file_path Path to AAC file
     * @param sample_rate Audio sample rate (e.g., 48000, 44100)
     */
    SessionAacWorkerThread(std::shared_ptr<lmshao::lmrtsp::RtspServerSession> session, const std::string &file_path,
                           uint32_t sample_rate);

    ~SessionAacWorkerThread();

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
    uint32_t sample_rate_;
    std::unique_ptr<SessionAacReader> reader_;
};

#endif // SESSION_AAC_WORKER_THREAD_H
