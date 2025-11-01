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

#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::lmrtsp {

class RtspSession;

// Session state base class - State pattern
class RtspSessionState {
public:
    virtual ~RtspSessionState() = default;

    // RTSP method handling
    virtual RtspResponse OnOptions(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnDescribe(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnAnnounce(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnRecord(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnSetup(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnPlay(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnPause(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnTeardown(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnGetParameter(RtspSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnSetParameter(RtspSession *session, const RtspRequest &request) = 0;

    // Get state name
    virtual std::string GetName() const = 0;
};

// Initial state - only accepts OPTIONS and DESCRIBE requests
class InitialState : public RtspSessionState, public lmcore::ManagedSingleton<InitialState> {
public:
    friend class lmcore::ManagedSingleton<InitialState>;

    RtspResponse OnOptions(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Initial"; }

protected:
    InitialState() = default;
};

// Ready state - SETUP completed, can accept PLAY requests
class ReadyState : public RtspSessionState, public lmcore::ManagedSingleton<ReadyState> {
public:
    friend class lmcore::ManagedSingleton<ReadyState>;

    RtspResponse OnOptions(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Ready"; }

protected:
    ReadyState() = default;
};

// Playing state - media stream is playing
class PlayingState : public RtspSessionState, public lmcore::ManagedSingleton<PlayingState> {
public:
    friend class lmcore::ManagedSingleton<PlayingState>;
    RtspResponse OnOptions(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Playing"; }

protected:
    PlayingState() = default;
};

// Paused state - media stream is paused
class PausedState : public RtspSessionState, public lmcore::ManagedSingleton<PausedState> {
public:
    friend class lmcore::ManagedSingleton<PausedState>;
    RtspResponse OnOptions(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Paused"; }

protected:
    PausedState() = default;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_SESSION_STATE_H