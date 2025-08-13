/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_SESSION_H
#define RTSP_SESSION_H

#include <network/session.h>

#include <memory>
#include <string>
#include <vector>

#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::rtsp {

class RTSPSessionState;
class MediaStream;

class RTSPSession {
public:
    explicit RTSPSession(std::shared_ptr<network::Session> networkSession);
    ~RTSPSession();

    // 处理RTSP请求
    RTSPResponse ProcessRequest(const RTSPRequest &request);

    // 状态管理
    void ChangeState(std::shared_ptr<RTSPSessionState> newState);
    std::shared_ptr<RTSPSessionState> GetCurrentState() const;

    // 会话信息
    std::string GetSessionId() const;
    std::shared_ptr<network::Session> GetNetworkSession() const;

    // 媒体管理
    bool SetupMedia(const std::string &uri, const std::string &transport);
    bool PlayMedia(const std::string &range = "");
    bool PauseMedia();
    bool TeardownMedia();

    // 资源管理
    void SetSdpDescription(const std::string &sdp);
    std::string GetSdpDescription() const;

    // 传输参数
    void SetTransportInfo(const std::string &transport);
    std::string GetTransportInfo() const;

private:
    // 生成唯一会话ID
    static std::string GenerateSessionId();

    std::string sessionId_;
    std::shared_ptr<RTSPSessionState> currentState_;
    std::shared_ptr<network::Session> networkSession_;

    // 媒体相关
    std::vector<std::shared_ptr<MediaStream>> mediaStreams_;
    std::string sdpDescription_;
    std::string transportInfo_;

    // 会话计时
    uint32_t timeout_;      // 会话超时时间（秒）
    time_t lastActiveTime_; // 最后活动时间
};

} // namespace lmshao::rtsp

#endif // RTSP_SESSION_H