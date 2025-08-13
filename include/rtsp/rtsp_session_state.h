/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_SESSION_STATE_H
#define RTSP_SESSION_STATE_H

#include <memory>

#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::rtsp {

class RTSPSession;

// 会话状态基类 - 状态模式
class RTSPSessionState {
public:
    virtual ~RTSPSessionState() = default;

    // RTSP方法处理
    virtual RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) = 0;

    // 获取状态名称
    virtual std::string GetName() const = 0;
};

// 初始状态 - 只接受OPTIONS和DESCRIBE请求
class InitialState : public RTSPSessionState {
public:
    static std::shared_ptr<RTSPSessionState> GetInstance();

    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Initial"; }

private:
    InitialState() = default;
};

// 就绪状态 - 已完成SETUP，可以接受PLAY请求
class ReadyState : public RTSPSessionState {
public:
    static std::shared_ptr<RTSPSessionState> GetInstance();

    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Ready"; }

private:
    ReadyState() = default;
};

// 播放状态 - 正在播放媒体流
class PlayingState : public RTSPSessionState {
public:
    static std::shared_ptr<RTSPSessionState> GetInstance();

    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Playing"; }

private:
    PlayingState() = default;
};

// 暂停状态 - 媒体流已暂停
class PausedState : public RTSPSessionState {
public:
    static std::shared_ptr<RTSPSessionState> GetInstance();

    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Paused"; }

private:
    PausedState() = default;
};

} // namespace lmshao::rtsp

#endif // RTSP_SESSION_STATE_H