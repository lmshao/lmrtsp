/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_SESSION_STATE_H
#define LMSHAO_LMRTSP_RTSP_SESSION_STATE_H

#include <lmcore/singleton.h>

#include <memory>

#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::lmrtsp {

class RTSPSession;

// Session state base class - State pattern
class RTSPSessionState {
public:
    virtual ~RTSPSessionState() = default;

    // RTSP method handling
    virtual RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnAnnounce(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnRecord(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) = 0;
    virtual RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) = 0;

    // Get state name
    virtual std::string GetName() const = 0;
};

// Initial state - only accepts OPTIONS and DESCRIBE requests
class InitialState : public RTSPSessionState, public lmcore::Singleton<InitialState> {
public:
    friend class lmcore::Singleton<InitialState>;

    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnAnnounce(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnRecord(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Initial"; }

protected:
    InitialState() = default;
};

// Ready state - SETUP completed, can accept PLAY requests
class ReadyState : public RTSPSessionState, public lmcore::Singleton<ReadyState> {
public:
    friend class lmcore::Singleton<ReadyState>;

    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnAnnounce(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnRecord(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Ready"; }

protected:
    ReadyState() = default;
};

// Playing state - media stream is playing
class PlayingState : public RTSPSessionState, public lmcore::Singleton<PlayingState> {
public:
    friend class lmcore::Singleton<PlayingState>;
    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnAnnounce(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnRecord(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Playing"; }

protected:
    PlayingState() = default;
};

// Paused state - media stream is paused
class PausedState : public RTSPSessionState, public lmcore::Singleton<PausedState> {
public:
    friend class lmcore::Singleton<PausedState>;
    RTSPResponse OnOptions(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnDescribe(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnAnnounce(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnRecord(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetup(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPlay(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnPause(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnTeardown(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnGetParameter(RTSPSession *session, const RTSPRequest &request) override;
    RTSPResponse OnSetParameter(RTSPSession *session, const RTSPRequest &request) override;

    std::string GetName() const override { return "Paused"; }

protected:
    PausedState() = default;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_SESSION_STATE_H