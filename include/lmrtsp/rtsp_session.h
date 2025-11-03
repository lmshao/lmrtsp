/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_SESSION_H
#define LMSHAO_LMRTSP_RTSP_SESSION_H

#include <lmcore/data_buffer.h>
#include <lmnet/iserver_listener.h>
#include <lmnet/session.h>
#include <lmnet/udp_server.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "lmrtsp/media_stream_info.h"
#include "lmrtsp/rtsp_media_stream_manager.h"
#include "lmrtsp/rtsp_request.h"
#include "lmrtsp/rtsp_response.h"

namespace lmshao::lmrtsp {

class RtspSessionState;
class MediaStream;
class RtspServer;

namespace rtsp {
class RtspMediaStreamManager;
struct MediaFrame;
} // namespace rtsp

class RtspSession : public std::enable_shared_from_this<RtspSession> {
public:
    explicit RtspSession(std::shared_ptr<lmnet::Session> lmnetSession);
    explicit RtspSession(std::shared_ptr<lmnet::Session> lmnetSession, std::weak_ptr<RtspServer> server);
    ~RtspSession();

    // Process RTSP request
    RtspResponse ProcessRequest(const RtspRequest &request);

    // State management
    void ChangeState(std::shared_ptr<RtspSessionState> newState);
    std::shared_ptr<RtspSessionState> GetCurrentState() const;

    // Session information
    std::string GetSessionId() const;
    std::string GetClientIP() const;
    uint16_t GetClientPort() const;
    std::shared_ptr<lmnet::Session> GetNetworkSession() const;
    std::weak_ptr<RtspServer> GetRTSPServer() const;

    // Media control
    bool SetupMedia(const std::string &uri, const std::string &transport);
    bool PlayMedia(const std::string &uri, const std::string &range = "");
    bool PauseMedia(const std::string &uri);
    bool TeardownMedia(const std::string &uri);
    std::shared_ptr<MediaStream> GetMediaStream(int track_index);
    const std::vector<std::shared_ptr<MediaStream>> &GetMediaStreams() const;

    // Media stream info
    void SetMediaStreamInfo(std::shared_ptr<MediaStreamInfo> stream_info);
    std::shared_ptr<MediaStreamInfo> GetMediaStreamInfo() const;

    // RTP sender management (removed by request)
    // void SetRTPSender(std::shared_ptr<IRTPSender> rtp_sender);
    // std::shared_ptr<IRTPSender> GetRTPSender() const;
    // bool HasRTPSender() const;

    // Transport parameters (removed by request)
    // void SetRtpTransportParams(const RtpTransportParams &params);
    // RtpTransportParams GetRtpTransportParams() const;
    // bool HasValidTransport() const;

    // SDP management
    void SetSdpDescription(const std::string &sdp);
    std::string GetSdpDescription() const;

    // Transport info (legacy)
    void SetTransportInfo(const std::string &transport);
    std::string GetTransportInfo() const;

    // State queries
    bool IsPlaying() const;
    bool IsPaused() const;
    bool IsSetup() const;

    // Statistics (removed by request)
    // RtpStatistics GetRtpStatistics() const;

    // Session timeout management
    void UpdateLastActiveTime();
    bool IsExpired(uint32_t timeout_seconds) const;
    int64_t GetLastActiveTime() const;

    // New media frame interface for RtspMediaStreamManager
    bool PushFrame(const lmrtsp::MediaFrame &frame);
    std::string GetRtpInfo() const;
    std::string GetStreamUri() const; // Get saved stream URI for RTP-Info in PLAY response

    // TCP interleaved data sending (for TcpInterleavedTransportAdapter)
    bool SendInterleavedData(uint8_t channel, const uint8_t *data, size_t size);

private:
    // Helper methods
    static std::string GenerateSessionId();

    // Transport parsing (removed by request)
    // RtpTransportParams ParseTransportHeader(const std::string &transport) const;

    std::string sessionId_;
    std::shared_ptr<RtspSessionState> currentState_;
    std::shared_ptr<lmnet::Session> lmnetSession_;
    std::weak_ptr<RtspServer> rtspServer_;

    // New media stream manager (replaces MediaStream)
    std::unique_ptr<lmshao::lmrtsp::RtspMediaStreamManager> mediaStreamManager_;
    mutable std::mutex mediaStreamManagerMutex_;

    // Legacy media streams (for backward compatibility)
    std::vector<std::shared_ptr<MediaStream>> mediaStreams_;
    mutable std::mutex mediaStreamsMutex_;
    std::string sdpDescription_;
    std::string transportInfo_; // legacy

    // Media stream info and RTP
    mutable std::mutex mediaInfoMutex_;
    std::shared_ptr<MediaStreamInfo> mediaStreamInfo_;
    // std::shared_ptr<IRTPSender> rtpSender_;
    // RtpTransportParams rtpTransportParams_;

    // State flags
    std::atomic<bool> isPlaying_{false};
    std::atomic<bool> isPaused_{false};
    std::atomic<bool> isSetup_{false};

    // Session timeout
    uint32_t timeout_;                    // Session timeout (seconds)
    std::atomic<int64_t> lastActiveTime_; // Last active time (milliseconds)

    // Stream URI for RTP-Info in PLAY response
    std::string streamUri_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_SESSION_H