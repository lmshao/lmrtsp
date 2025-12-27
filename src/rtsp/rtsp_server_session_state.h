/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_SERVER_SESSION_STATE_H
#define LMSHAO_LMRTSP_RTSP_SERVER_SESSION_STATE_H

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
    virtual RtspResponse OnOptionsRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnDescribeRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnRecordRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnSetupRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnPlayRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnPauseRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnTeardownRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request) = 0;
    virtual RtspResponse OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request) = 0;

    // Get state name
    virtual std::string GetName() const = 0;
};

// Initial state - only accepts OPTIONS and DESCRIBE requests
class ServerInitialState : public RtspServerSessionState, public lmcore::Singleton<ServerInitialState> {
public:
    friend class lmcore::Singleton<ServerInitialState>;

    RtspResponse OnOptionsRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribeRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecordRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetupRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlayRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPauseRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardownRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Initial"; }

protected:
    ServerInitialState() = default;
};

// Ready state - SETUP completed, can accept PLAY requests
class ServerReadyState : public RtspServerSessionState, public lmcore::Singleton<ServerReadyState> {
public:
    friend class lmcore::Singleton<ServerReadyState>;

    RtspResponse OnOptionsRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribeRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecordRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetupRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlayRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPauseRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardownRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Ready"; }

protected:
    ServerReadyState() = default;
};

// Playing state - media stream is playing
class ServerPlayingState : public RtspServerSessionState, public lmcore::Singleton<ServerPlayingState> {
public:
    friend class lmcore::Singleton<ServerPlayingState>;
    RtspResponse OnOptionsRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribeRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecordRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetupRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlayRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPauseRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardownRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Playing"; }

protected:
    ServerPlayingState() = default;
};

// Paused state - media stream is paused
class ServerPausedState : public RtspServerSessionState, public lmcore::Singleton<ServerPausedState> {
public:
    friend class lmcore::Singleton<ServerPausedState>;
    RtspResponse OnOptionsRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnDescribeRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnRecordRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetupRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPlayRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnPauseRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnTeardownRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;
    RtspResponse OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request) override;

    std::string GetName() const override { return "Paused"; }

protected:
    ServerPausedState() = default;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_SERVER_SESSION_STATE_H