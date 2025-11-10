/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_SERVER_SESSION_H
#define LMSHAO_LMRTSP_RTSP_SERVER_SESSION_H

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

class RtspServerSessionState;
class MediaStream;
class RtspServer;
class RtspMediaStreamManager;
struct MediaFrame;

class RtspServerSession : public std::enable_shared_from_this<RtspServerSession> {
public:
    explicit RtspServerSession(std::shared_ptr<lmnet::Session> lmnetSession);
    explicit RtspServerSession(std::shared_ptr<lmnet::Session> lmnetSession, std::weak_ptr<RtspServer> server);
    ~RtspServerSession();

    // Process RTSP request
    RtspResponse ProcessRequest(const RtspRequest &request);

    // State management
    void ChangeState(std::shared_ptr<RtspServerSessionState> newState);
    std::shared_ptr<RtspServerSessionState> GetCurrentState() const;

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

    // Session timeout management
    void UpdateLastActiveTime();
    bool IsExpired(uint32_t timeout_seconds) const;
    int64_t GetLastActiveTime() const;

    // New media frame interface for RtspMediaStreamManager
    bool PushFrame(const lmrtsp::MediaFrame &frame);
    bool PushFrame(const lmrtsp::MediaFrame &frame, int track_index); // Multi-track version
    std::string GetRtpInfo() const;
    std::string GetStreamUri() const; // Get saved stream URI for RTP-Info in PLAY response

    // TCP interleaved data sending (for TcpInterleavedTransportAdapter)
    bool SendInterleavedData(uint8_t channel, const uint8_t *data, size_t size);

    // Multi-track support: get track information
    struct TrackInfo {
        std::string uri; // Track URI (e.g., /file.mkv/track0)
        std::shared_ptr<MediaStreamInfo> stream_info;
        int track_index = -1; // Track index from SDP (0, 1, 2, ...)
    };
    std::vector<TrackInfo> GetTracks() const;
    bool IsMultiTrack() const;

private:
    // Helper methods
    static std::string GenerateSessionId();

    std::string sessionId_;
    std::shared_ptr<RtspServerSessionState> currentState_;
    std::shared_ptr<lmnet::Session> lmnetSession_;
    std::weak_ptr<RtspServer> rtspServer_;

    // Multi-track support: internal track info for each media stream
    struct InternalTrackInfo {
        std::string uri; // Track URI (e.g., /file.mkv/track0)
        std::shared_ptr<MediaStreamInfo> stream_info;
        std::unique_ptr<lmshao::lmrtsp::RtspMediaStreamManager> stream_manager;
        std::string transport_info;
        int track_index = -1; // Track index from SDP (0, 1, 2, ...)
    };

    // Map of track index to InternalTrackInfo
    std::map<int, InternalTrackInfo> tracks_;
    mutable std::mutex tracksMutex_;

    // Legacy single-track support (for backward compatibility)
    std::unique_ptr<lmshao::lmrtsp::RtspMediaStreamManager> mediaStreamManager_;
    mutable std::mutex mediaStreamManagerMutex_;

    // Legacy media streams (for backward compatibility)
    std::vector<std::shared_ptr<MediaStream>> mediaStreams_;
    mutable std::mutex mediaStreamsMutex_;
    std::string sdpDescription_;
    std::string transportInfo_; // legacy

    // Primary media stream info (for single-track or main track)
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

#endif // LMSHAO_LMRTSP_RTSP_SERVER_SESSION_H