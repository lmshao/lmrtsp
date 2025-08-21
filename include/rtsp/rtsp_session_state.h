/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_SESSION_STATE_H
#define RTSP_SESSION_STATE_H

#include <coreutils/singleton.h>

#include <memory>

#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::rtsp {

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
class InitialState : public RTSPSessionState, public coreutils::Singleton<InitialState> {
public:
    friend class coreutils::Singleton<InitialState>;

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
class ReadyState : public RTSPSessionState, public coreutils::Singleton<ReadyState> {
public:
    friend class coreutils::Singleton<ReadyState>;

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
class PlayingState : public RTSPSessionState, public coreutils::Singleton<PlayingState> {
public:
    friend class coreutils::Singleton<PlayingState>;
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
class PausedState : public RTSPSessionState, public coreutils::Singleton<PausedState> {
public:
    friend class coreutils::Singleton<PausedState>;
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

} // namespace lmshao::rtsp

#endif // RTSP_SESSION_STATE_H