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

#include <memory>
#include <string>
#include <vector>

#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::rtsp {

class RTSPSessionState;
class MediaStream;

class RTSPSession : public std::enable_shared_from_this<RTSPSession> {
public:
    explicit RTSPSession(std::shared_ptr<network::Session> networkSession);
    ~RTSPSession();

    // Process RTSP request
    RTSPResponse ProcessRequest(const RTSPRequest &request);

    // State management
    void ChangeState(std::shared_ptr<RTSPSessionState> newState);
    std::shared_ptr<RTSPSessionState> GetCurrentState() const;

    // Session information
    std::string GetSessionId() const;
    std::shared_ptr<network::Session> GetNetworkSession() const;

    // Media management
    bool SetupMedia(const std::string &uri, const std::string &transport);
    bool PlayMedia(const std::string &uri, const std::string &range = "");
    bool PauseMedia(const std::string &uri);
    bool TeardownMedia(const std::string &uri);
    std::shared_ptr<MediaStream> GetMediaStream(int track_index);
    const std::vector<std::shared_ptr<MediaStream>> &GetMediaStreams() const;

    // Resource management
    void SetSdpDescription(const std::string &sdp);
    std::string GetSdpDescription() const;

    // Transport parameters
    void SetTransportInfo(const std::string &transport);
    std::string GetTransportInfo() const;

private:
    // Generate unique session ID
    static std::string GenerateSessionId();

    std::string sessionId_;
    std::shared_ptr<RTSPSessionState> currentState_;
    std::shared_ptr<network::Session> networkSession_;

    // Media related
    std::vector<std::shared_ptr<MediaStream>> mediaStreams_;
    std::string sdpDescription_;
    std::string transportInfo_;

    // Session timing
    uint32_t timeout_;      // Session timeout (seconds)
    time_t lastActiveTime_; // Last active time
};

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_RTSP_SESSION_H