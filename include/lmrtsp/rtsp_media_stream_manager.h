/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_RTSP_MEDIA_STREAM_MANAGER_H
#define LMSHAO_RTSP_RTSP_MEDIA_STREAM_MANAGER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "../src/rtp/i_rtp_transport_adapter.h"
#include "lmrtsp/media_types.h"

// #include "i_rtp_packetizer.h"
// #include "rtp_session.h"
// #include "rtp_transport_adapter.h"

namespace lmshao {
namespace lmrtsp {
class RTSPSession;
// Add forward declaration for RtpSession
class RtpSession;

// Dummy media frame for build-only mode
// struct MediaFrame {
//     std::vector<uint8_t> data{};
//     uint32_t timestamp = 0;
//     bool marker = false;
// };
} // namespace lmrtsp
} // namespace lmshao

namespace lmshao::lmrtsp {
// Stream state enumeration and RtspMediaStreamManager should be inside this namespace
enum class StreamState {
    IDLE,
    SETUP,
    PLAYING,
    PAUSED
};

/**
 * Media stream manager for RTSP sessions
 * Replaces the functionality of RTPStream with better separation of concerns
 */
class RtspMediaStreamManager {
public:
    explicit RtspMediaStreamManager(std::weak_ptr<lmshao::lmrtsp::RTSPSession> rtsp_session);
    ~RtspMediaStreamManager();

    /**
     * Setup the media stream with transport configuration
     * @param config Transport configuration
     * @return true if setup successful, false otherwise
     */
    bool Setup(const lmshao::lmrtsp::TransportConfig &config);

    /**
     * Start playing the media stream
     * @return true if started successfully, false otherwise
     */
    bool Play();

    /**
     * Pause the media stream
     * @return true if paused successfully, false otherwise
     */
    bool Pause();

    /**
     * Teardown the media stream and release resources
     */
    void Teardown();

    /**
     * Push a media frame to the stream queue
     * @param frame Media frame to push
     * @return true if pushed successfully, false otherwise
     */
    bool PushFrame(const lmrtsp::MediaFrame &frame);

    /**
     * Get RTP information for RTSP response
     * @return RTP info string
     */
    std::string GetRtpInfo() const;

    /**
     * Get transport information for RTSP response
     * @return Transport info string
     */
    std::string GetTransportInfo() const;

    /**
     * Get current stream state
     * @return Current state
     */
    StreamState GetState() const { return state_; }

    /**
     * Check if stream is active
     * @return true if active, false otherwise
     */
    bool IsActive() const;

private:
    /**
     * Media sending thread function
     */
    void SendMediaThread();

    /**
     * Process a single frame from the queue
     * @param frame Frame to process
     */
    void ProcessFrame(const lmrtsp::MediaFrame &frame);

    /**
     * Create transport adapter based on configuration
     * @param config Transport configuration
     * @return Unique pointer to transport adapter
     */
    std::unique_ptr<lmshao::lmrtsp::IRtpTransportAdapter>
    CreateTransportAdapter(const lmshao::lmrtsp::TransportConfig &config);

    std::weak_ptr<lmshao::lmrtsp::RTSPSession> rtsp_session_;
    std::shared_ptr<lmrtsp::RtpSession> rtp_session_;
    std::unique_ptr<lmshao::lmrtsp::IRtpTransportAdapter> transport_adapter_;

    // Persist the transport config to build proper Transport header
    lmshao::lmrtsp::TransportConfig transport_config_{};

    StreamState state_;
    std::atomic<bool> active_;
    std::atomic<bool> send_thread_running_;

    // Media frame queue
    std::queue<lmrtsp::MediaFrame> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;

    // Send thread
    std::unique_ptr<std::thread> send_thread_;

    // RTP parameters
    uint16_t sequence_number_;
    uint32_t timestamp_;
    uint32_t ssrc_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_RTSP_RTSP_MEDIA_STREAM_MANAGER_H