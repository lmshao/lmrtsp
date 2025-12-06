/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_H264_WORKER_THREAD_H
#define LMSHAO_RTSP_SESSION_H264_WORKER_THREAD_H

#include <lmcore/data_buffer.h>
#include <lmrtsp/media_types.h>
#include <lmrtsp/rtsp_server_session.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include "base_session_worker_thread.h"
#include "file_manager.h"
#include "session_h264_reader.h"

using namespace lmshao::lmrtsp;
using namespace lmshao::lmcore;

/**
 * @brief Worker thread for handling H.264 RTSP client session
 *
 * Each SessionH264WorkerThread manages one client session independently:
 * - Uses shared MappedFile through FileManager for efficient memory usage
 * - Maintains independent playback progress with SessionH264Reader
 * - Runs in its own thread for concurrent client support
 * - Handles frame timing and streaming control
 */
class SessionH264WorkerThread : public BaseSessionWorkerThread {
public:
    /**
     * @brief Constructor
     * @param session RTSP session to handle
     * @param file_path Path to the H.264 file
     * @param frame_rate Target frame rate for streaming (fps)
     */
    SessionH264WorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                            uint32_t frame_rate = 25);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~SessionH264WorkerThread();

    /**
     * @brief Get current playback information
     * @return Playback info structure
     */
    SessionH264Reader::PlaybackInfo GetPlaybackInfo() const;

    /**
     * @brief Seek to specific frame
     * @param frame_index Target frame index
     * @return true if seek successful, false otherwise
     */
    bool SeekToFrame(size_t frame_index);

    /**
     * @brief Seek to specific time
     * @param timestamp Target timestamp in seconds
     * @return true if seek successful, false otherwise
     */
    bool SeekToTime(double timestamp);

    /**
     * @brief Reset playback to beginning
     */
    void Reset() override;

    /**
     * @brief Set frame rate for streaming
     * @param fps Frames per second
     */
    void SetFrameRate(uint32_t fps);

    /**
     * @brief Get current frame rate
     * @return Frames per second
     */
    uint32_t GetFrameRate() const;

protected:
    /**
     * @brief Initialize the reader (called by Start())
     * @return true if successful, false otherwise
     */
    bool InitializeReader() override;

    /**
     * @brief Send next frame to client
     * @return true if frame sent successfully, false if EOF or error
     */
    bool SendNextData() override;

    /**
     * @brief Calculate frame interval based on frame rate
     * @return Frame interval in microseconds
     */
    std::chrono::microseconds GetDataInterval() const override;

    /**
     * @brief Reset the reader to beginning
     */
    void ResetReader() override;

    /**
     * @brief Cleanup reader resources (called by Stop())
     */
    void CleanupReader() override;

    /**
     * @brief Release file resources (called by Stop())
     */
    void ReleaseFile() override;

private:
    /**
     * @brief Send next frame to client (internal implementation)
     * @return true if frame sent successfully, false if EOF or error
     */
    bool SendNextFrame();

    // H.264 reader for independent playback
    std::unique_ptr<SessionH264Reader> h264_reader_;

    // Streaming parameters
    std::atomic<uint32_t> frame_rate_;
    std::atomic<uint64_t> frame_counter_;
};

#endif // LMSHAO_RTSP_SESSION_H264_WORKER_THREAD_H