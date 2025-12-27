/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_CLIENT_STATE_H
#define LMSHAO_LMRTSP_RTSP_CLIENT_STATE_H

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

// Forward declaration
class RtspClient;

// Client session state machine base class - State pattern
class RtspClientStateMachine {
public:
    virtual ~RtspClientStateMachine() = default;

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
class ClientInitState : public RtspClientStateMachine, public lmcore::ManagedSingleton<ClientInitState> {
public:
    friend class lmcore::ManagedSingleton<ClientInitState>;

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

    std::string GetName() const override { return "Init"; }

protected:
    ClientInitState() = default;
};

// Options sent state - waiting for OPTIONS response, then send DESCRIBE
class ClientOptionsSentState : public RtspClientStateMachine, public lmcore::ManagedSingleton<ClientOptionsSentState> {
public:
    friend class lmcore::ManagedSingleton<ClientOptionsSentState>;

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
class ClientDescribeSentState : public RtspClientStateMachine,
                                public lmcore::ManagedSingleton<ClientDescribeSentState> {
public:
    friend class lmcore::ManagedSingleton<ClientDescribeSentState>;

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
class ClientSetupSentState : public RtspClientStateMachine, public lmcore::ManagedSingleton<ClientSetupSentState> {
public:
    friend class lmcore::ManagedSingleton<ClientSetupSentState>;

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
class ClientPlaySentState : public RtspClientStateMachine, public lmcore::ManagedSingleton<ClientPlaySentState> {
public:
    friend class lmcore::ManagedSingleton<ClientPlaySentState>;

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
class ClientPlayingState : public RtspClientStateMachine, public lmcore::ManagedSingleton<ClientPlayingState> {
public:
    friend class lmcore::ManagedSingleton<ClientPlayingState>;

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

#endif // LMSHAO_LMRTSP_RTSP_CLIENT_STATE_H
