/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_RTSP_SESSION_H
#define LMSHAO_RTSP_RTSP_SESSION_H

#include <coreutils/data_buffer.h>
#include <network/iserver_listener.h>
#include <network/session.h>
#include <network/udp_server.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "irtp_sender.h"
#include "media_stream_info.h"
#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::rtsp {

class RTSPSessionState;
class MediaStream;
class RTSPServer;

class RTSPSession : public std::enable_shared_from_this<RTSPSession> {
public:
    explicit RTSPSession(std::shared_ptr<network::Session> networkSession);
    explicit RTSPSession(std::shared_ptr<network::Session> networkSession, std::weak_ptr<RTSPServer> server);
    ~RTSPSession();

    // Process RTSP request
    RTSPResponse ProcessRequest(const RTSPRequest &request);

    // State management
    void ChangeState(std::shared_ptr<RTSPSessionState> newState);
    std::shared_ptr<RTSPSessionState> GetCurrentState() const;

    // Session information
    std::string GetSessionId() const;
    std::string GetClientIP() const;
    uint16_t GetClientPort() const;
    std::shared_ptr<network::Session> GetNetworkSession() const;
    std::weak_ptr<RTSPServer> GetRTSPServer() const;

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

    // RTP sender management
    void SetRTPSender(std::shared_ptr<IRTPSender> rtp_sender);
    std::shared_ptr<IRTPSender> GetRTPSender() const;
    bool HasRTPSender() const;

    // Transport parameters
    void SetRTPTransportParams(const RTPTransportParams &params);
    RTPTransportParams GetRTPTransportParams() const;
    bool HasValidTransport() const;

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

    // Statistics
    RTPStatistics GetRTPStatistics() const;

    // Session timeout management
    void UpdateLastActiveTime();
    bool IsExpired(uint32_t timeout_seconds) const;
    time_t GetLastActiveTime() const;

private:
    // Helper methods
    static std::string GenerateSessionId();

    // Transport parsing
    RTPTransportParams ParseTransportHeader(const std::string &transport) const;

    std::string sessionId_;
    std::shared_ptr<RTSPSessionState> currentState_;
    std::shared_ptr<network::Session> networkSession_;
    std::weak_ptr<RTSPServer> rtspServer_;

    // Media streams
    std::vector<std::shared_ptr<MediaStream>> mediaStreams_;
    std::string sdpDescription_;
    std::string transportInfo_; // legacy

    // Media stream info and RTP
    mutable std::mutex mediaInfoMutex_;
    std::shared_ptr<MediaStreamInfo> mediaStreamInfo_;
    std::shared_ptr<IRTPSender> rtpSender_;
    RTPTransportParams rtpTransportParams_;

    // State flags
    std::atomic<bool> isPlaying_{false};
    std::atomic<bool> isPaused_{false};
    std::atomic<bool> isSetup_{false};

    // Session timeout
    uint32_t timeout_;                   // Session timeout (seconds)
    std::atomic<time_t> lastActiveTime_; // Last active time
};

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_RTSP_SESSION_H