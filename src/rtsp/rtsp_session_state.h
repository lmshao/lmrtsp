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

class RtspServerSession;

// Session state base class - State pattern
class RtspServerSessionState {
public:
    virtual ~RtspServerSessionState() = default;

    // RTSP method handling
    virtual RtspResponse OnOptions(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnDescribe(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnAnnounce(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnRecord(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnSetup(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnPlay(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnPause(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnTeardown(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnGetParameter(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnSetParameter(RtspServerSession *session, const RtspRequest &request) = 0;

    // Get state name
    virtual std::string GetName() const = 0;
};

// Initial state - only accepts OPTIONS and DESCRIBE requests
class InitialState : public RtspServerSessionState, public lmcore::ManagedSingleton<InitialState> {
public:
    friend class lmcore::ManagedSingleton<InitialState>;

    RtspResponse OnOptions(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Initial"; }

protected:
    InitialState() = default;
};

// Ready state - SETUP completed, can accept PLAY requests
class ReadyState : public RtspServerSessionState, public lmcore::ManagedSingleton<ReadyState> {
public:
    friend class lmcore::ManagedSingleton<ReadyState>;

    RtspResponse OnOptions(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Ready"; }

protected:
    ReadyState() = default;
};

// Playing state - media stream is playing
class PlayingState : public RtspServerSessionState, public lmcore::ManagedSingleton<PlayingState> {
public:
    friend class lmcore::ManagedSingleton<PlayingState>;
    RtspResponse OnOptions(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Playing"; }

protected:
    PlayingState() = default;
};

// Paused state - media stream is paused
class PausedState : public RtspServerSessionState, public lmcore::ManagedSingleton<PausedState> {
public:
    friend class lmcore::ManagedSingleton<PausedState>;
    RtspResponse OnOptions(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribe(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounce(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecord(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetup(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlay(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPause(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardown(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameter(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameter(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Paused"; }

protected:
    PausedState() = default;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_SESSION_STATE_H