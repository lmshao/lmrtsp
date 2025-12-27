/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_CLIENT_SESSION_STATE_H
#define LMSHAO_LMRTSP_RTSP_CLIENT_SESSION_STATE_H

#include <lmcore/singleton.h>

#include "rtsp_response.h"

namespace lmshao::lmrtsp {

class RtspClientSession;
class RtspClient;

// Next action after processing response
enum class ClientStateAction {
    CONTINUE, // Continue to next step (send next request)
    WAIT,     // Wait for more data
    FAIL,     // Failed, abort handshake
    SUCCESS   // Handshake completed successfully
};

// Client session state machine base class - State pattern
class RtspClientSessionState {
public:
    virtual ~RtspClientSessionState() = default;

    // RTSP response handling - returns action to take next
    virtual ClientStateAction OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                                const RtspResponse &response) = 0;
    virtual ClientStateAction OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                                 const RtspResponse &response) = 0;
    virtual ClientStateAction OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                              const RtspResponse &response) = 0;
    virtual ClientStateAction OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                             const RtspResponse &response) = 0;
    virtual ClientStateAction OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                              const RtspResponse &response) = 0;
    virtual ClientStateAction OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                                 const RtspResponse &response) = 0;

    // Get state name
    virtual std::string GetName() const = 0;
};

// Initial state - waiting for OPTIONS or DESCRIBE
class ClientInitialState : public RtspClientSessionState, public lmcore::Singleton<ClientInitialState> {
public:
    friend class lmcore::Singleton<ClientInitialState>;

    ClientStateAction OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                        const RtspResponse &response) override;
    ClientStateAction OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;
    ClientStateAction OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                     const RtspResponse &response) override;
    ClientStateAction OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;

    std::string GetName() const override { return "Initial"; }

protected:
    ClientInitialState() = default;
};

// Options sent state - waiting for OPTIONS response, then send DESCRIBE
class ClientOptionsSentState : public RtspClientSessionState, public lmcore::Singleton<ClientOptionsSentState> {
public:
    friend class lmcore::Singleton<ClientOptionsSentState>;

    ClientStateAction OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                        const RtspResponse &response) override;
    ClientStateAction OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;
    ClientStateAction OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                     const RtspResponse &response) override;
    ClientStateAction OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;

    std::string GetName() const override { return "OptionsSent"; }

protected:
    ClientOptionsSentState() = default;
};

// Describe sent state - waiting for DESCRIBE response, then send SETUP
class ClientDescribeSentState : public RtspClientSessionState, public lmcore::Singleton<ClientDescribeSentState> {
public:
    friend class lmcore::Singleton<ClientDescribeSentState>;

    ClientStateAction OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                        const RtspResponse &response) override;
    ClientStateAction OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;
    ClientStateAction OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                     const RtspResponse &response) override;
    ClientStateAction OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;

    std::string GetName() const override { return "DescribeSent"; }

protected:
    ClientDescribeSentState() = default;
};

// Setup sent state - waiting for SETUP response, then send PLAY
class ClientSetupSentState : public RtspClientSessionState, public lmcore::Singleton<ClientSetupSentState> {
public:
    friend class lmcore::Singleton<ClientSetupSentState>;

    ClientStateAction OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                        const RtspResponse &response) override;
    ClientStateAction OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;
    ClientStateAction OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                     const RtspResponse &response) override;
    ClientStateAction OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;

    std::string GetName() const override { return "SetupSent"; }

protected:
    ClientSetupSentState() = default;
};

// Play sent state - waiting for PLAY response
class ClientPlaySentState : public RtspClientSessionState, public lmcore::Singleton<ClientPlaySentState> {
public:
    friend class lmcore::Singleton<ClientPlaySentState>;

    ClientStateAction OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                        const RtspResponse &response) override;
    ClientStateAction OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;
    ClientStateAction OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                     const RtspResponse &response) override;
    ClientStateAction OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;

    std::string GetName() const override { return "PlaySent"; }

protected:
    ClientPlaySentState() = default;
};

// Playing state - media is playing
class ClientPlayingState : public RtspClientSessionState, public lmcore::Singleton<ClientPlayingState> {
public:
    friend class lmcore::Singleton<ClientPlayingState>;

    ClientStateAction OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                        const RtspResponse &response) override;
    ClientStateAction OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;
    ClientStateAction OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                     const RtspResponse &response) override;
    ClientStateAction OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                      const RtspResponse &response) override;
    ClientStateAction OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                         const RtspResponse &response) override;

    std::string GetName() const override { return "Playing"; }

protected:
    ClientPlayingState() = default;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_CLIENT_SESSION_STATE_H
