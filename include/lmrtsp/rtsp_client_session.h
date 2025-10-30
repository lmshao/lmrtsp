/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_CLIENT_SESSION_H
#define LMSHAO_LMRTSP_RTSP_CLIENT_SESSION_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "lmrtsp/media_stream_info.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtp_sink_session.h"
#include "lmrtsp/transport_config.h"

namespace lmshao::lmrtsp {

class RTSPClient;
class RtpSinkSession;

/**
 * @brief RTSP Client Session state
 */
enum class RTSPClientSessionState {
    INIT,    // Initial state
    READY,   // SETUP completed, ready to play
    PLAYING, // Playing media
    PAUSED,  // Media paused
    TEARDOWN // Session terminated
};

/**
 * @brief RTSP Client Session class
 *
 * This class manages a single RTSP session with a server, maintaining
 * session state and handling RTP reception.
 */
class RTSPClientSession : public RtpSinkSessionListener, public std::enable_shared_from_this<RTSPClientSession> {
public:
    explicit RTSPClientSession(const std::string &url, std::weak_ptr<RTSPClient> client);
    ~RTSPClientSession();

    // Session lifecycle
    bool Initialize();
    void Cleanup();

    // RTSP method handlers
    bool HandleDescribeResponse(const std::string &sdp);
    bool HandleSetupResponse(const std::string &session_id, const std::string &transport);
    bool HandlePlayResponse(const std::string &rtp_info = "");
    bool HandlePauseResponse();
    bool HandleTeardownResponse();

    // State management
    void SetState(RTSPClientSessionState new_state);
    RTSPClientSessionState GetState() const;
    std::string GetStateString() const;
    static std::string GetStateString(RTSPClientSessionState state);

    // Session information
    std::string GetSessionId() const { return sessionId_; }
    std::string GetUrl() const { return url_; }
    std::string GetTransportInfo() const { return transportInfo_; }
    std::string GetSdpDescription() const { return sdpDescription_; }

    // Media information
    std::shared_ptr<MediaStreamInfo> GetMediaStreamInfo() const;
    std::string GetMediaPath() const;

    // RTP session management
    std::shared_ptr<RtpSinkSession> GetRtpSession() const { return rtpSession_; }
    bool StartRtpSession();
    void StopRtpSession();

    // Transport configuration
    void SetTransportConfig(const TransportConfig &config);
    TransportConfig GetTransportConfig() const;

    // Statistics
    size_t GetFramesReceived() const { return framesReceived_; }
    size_t GetBytesReceived() const { return bytesReceived_; }

    // RtpSinkSessionListener implementation
    void OnFrame(const std::shared_ptr<MediaFrame> &frame) override;
    void OnError(int code, const std::string &message) override;

private:
    // Helper methods
    bool ParseSDP(const std::string &sdp);
    bool SetupRtpSession();
    std::string AllocateClientPorts();
    std::string GenerateTransportHeader();

    // Session data
    std::string url_;
    std::string sessionId_;
    std::weak_ptr<RTSPClient> client_;
    std::atomic<RTSPClientSessionState> state_{RTSPClientSessionState::INIT};

    // Media information
    std::string sdpDescription_;
    std::shared_ptr<MediaStreamInfo> mediaStreamInfo_;
    std::string mediaPath_;

    // Transport information
    std::string transportInfo_;
    TransportConfig transportConfig_;
    uint16_t clientRtpPort_ = 0;
    uint16_t clientRtcpPort_ = 0;

    // RTP session
    std::shared_ptr<RtpSinkSession> rtpSession_;
    bool rtpSessionStarted_ = false;

    // Statistics
    std::atomic<size_t> framesReceived_{0};
    std::atomic<size_t> bytesReceived_{0};

    // Thread safety
    mutable std::mutex sessionMutex_;
    mutable std::mutex statsMutex_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_CLIENT_SESSION_H