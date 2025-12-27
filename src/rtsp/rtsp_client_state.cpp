/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_client_state.h"

#include "internal_logger.h"
#include "lmrtsp/rtsp_client.h"
#include "lmrtsp/rtsp_client_session.h"
#include "rtsp_response.h"

namespace lmshao::lmrtsp {

// ClientInitState implementation
ClientStateAction ClientInitState::OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                                     const RtspResponse &response)
{
    LMRTSP_LOGD("ClientInitState: Received OPTIONS response");
    // Transition to OptionsSent state
    session->ChangeState(ClientOptionsSentState::GetInstance());

    if (response.status_ == StatusCode::OK) {
        // OPTIONS succeeded, send DESCRIBE next
        LMRTSP_LOGI("OPTIONS succeeded, sending DESCRIBE");
        if (client && session) {
            std::string url = session->GetUrl();
            LMRTSP_LOGD("Calling client->SendDescribeRequest(%s)", url.c_str());
            bool describe_result = client->SendDescribeRequest(url);
            LMRTSP_LOGD("client->SendDescribeRequest() returned: %s", describe_result ? "true" : "false");
            if (describe_result) {
                // Transition to DescribeSent state after sending DESCRIBE
                session->ChangeState(ClientDescribeSentState::GetInstance());
                return ClientStateAction::CONTINUE;
            }
        } else {
            LMRTSP_LOGE("client or session is null!");
        }
        return ClientStateAction::FAIL;
    }
    // Even if OPTIONS fails, we can still try DESCRIBE (OPTIONS is optional)
    LMRTSP_LOGW("OPTIONS failed, but trying DESCRIBE anyway");
    if (client && session) {
        std::string url = session->GetUrl();
        if (client->SendDescribeRequest(url)) {
            // Transition to DescribeSent state after sending DESCRIBE
            session->ChangeState(ClientDescribeSentState::GetInstance());
            return ClientStateAction::CONTINUE;
        }
    }
    return ClientStateAction::FAIL;
}

ClientStateAction ClientInitState::OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                                      const RtspResponse &response)
{
    LMRTSP_LOGD("ClientInitState: Received DESCRIBE response (unexpected in Init state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientInitState::OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                                   const RtspResponse &response)
{
    LMRTSP_LOGD("ClientInitState: Received SETUP response (unexpected in Init state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientInitState::OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                                  const RtspResponse &response)
{
    LMRTSP_LOGD("ClientInitState: Received PLAY response (unexpected in Init state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientInitState::OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                                   const RtspResponse &response)
{
    LMRTSP_LOGD("ClientInitState: Received PAUSE response (unexpected in Init state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientInitState::OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                                      const RtspResponse &response)
{
    LMRTSP_LOGD("ClientInitState: Received TEARDOWN response");
    return ClientStateAction::SUCCESS;
}

// ClientOptionsSentState implementation
ClientStateAction ClientOptionsSentState::OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                                            const RtspResponse &response)
{
    LMRTSP_LOGD("ClientOptionsSentState: Received OPTIONS response");

    if (response.status_ == StatusCode::OK) {
        // OPTIONS succeeded, now send DESCRIBE
        if (client && session) {
            std::string url = session->GetUrl();
            if (client->SendDescribeRequest(url)) {
                // Transition to DescribeSent state after sending DESCRIBE
                session->ChangeState(ClientDescribeSentState::GetInstance());
                return ClientStateAction::CONTINUE;
            }
        }
        return ClientStateAction::FAIL;
    }
    // Even if OPTIONS fails, we can still try DESCRIBE (OPTIONS is optional)
    if (client && session) {
        std::string url = session->GetUrl();
        if (client->SendDescribeRequest(url)) {
            // Transition to DescribeSent state after sending DESCRIBE
            session->ChangeState(ClientDescribeSentState::GetInstance());
            return ClientStateAction::CONTINUE;
        }
    }
    return ClientStateAction::FAIL;
}

ClientStateAction ClientOptionsSentState::OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                                             const RtspResponse &response)
{
    LMRTSP_LOGD("ClientOptionsSentState: Received DESCRIBE response (unexpected in OptionsSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientOptionsSentState::OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                                          const RtspResponse &response)
{
    LMRTSP_LOGD("ClientOptionsSentState: Received SETUP response (unexpected in OptionsSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientOptionsSentState::OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                                         const RtspResponse &response)
{
    LMRTSP_LOGD("ClientOptionsSentState: Received PLAY response (unexpected in OptionsSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientOptionsSentState::OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                                          const RtspResponse &response)
{
    LMRTSP_LOGD("ClientOptionsSentState: Received PAUSE response (unexpected in OptionsSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientOptionsSentState::OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                                             const RtspResponse &response)
{
    LMRTSP_LOGD("ClientOptionsSentState: Received TEARDOWN response");
    return ClientStateAction::SUCCESS;
}

// ClientDescribeSentState implementation
ClientStateAction ClientDescribeSentState::OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                                             const RtspResponse &response)
{
    LMRTSP_LOGD("ClientDescribeSentState: Received OPTIONS response (unexpected in DescribeSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientDescribeSentState::OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                                              const RtspResponse &response)
{
    LMRTSP_LOGD("ClientDescribeSentState: Received DESCRIBE response");
    if (response.status_ == StatusCode::OK) {
        // DESCRIBE succeeded, now send SETUP
        if (client && session) {
            std::string url = session->GetUrl();
            std::string control_url = session->GetControlUrl();

            // Construct SETUP URL based on control URL from SDP
            std::string setup_url;
            if (control_url.empty() || control_url == "*") {
                // No specific control URL or aggregate control, use base URL
                setup_url = url;
                LMRTSP_LOGI("Using base URL for SETUP (control='%s'): %s", control_url.c_str(), setup_url.c_str());
            } else if (control_url.find("rtsp://") == 0) {
                // Absolute URL
                setup_url = control_url;
                LMRTSP_LOGI("Using absolute control URL for SETUP: %s", setup_url.c_str());
            } else {
                // Relative URL, append to base URL
                if (!url.empty() && url.back() != '/') {
                    setup_url = url + "/" + control_url;
                } else {
                    setup_url = url + control_url;
                }
                LMRTSP_LOGI("Using relative control URL for SETUP: %s (base: %s, control: %s)", setup_url.c_str(),
                            url.c_str(), control_url.c_str());
            }

            std::string transport = session->GetTransportInfo();
            if (transport.empty()) {
                transport = "RTP/AVP;unicast;client_port=5000-5001";
            }

            if (client->SendSetupRequest(setup_url, transport)) {
                // Transition to SetupSent state after sending SETUP
                session->ChangeState(ClientSetupSentState::GetInstance());
                return ClientStateAction::CONTINUE;
            }
        }
        return ClientStateAction::FAIL;
    }
    return ClientStateAction::FAIL;
}

ClientStateAction ClientDescribeSentState::OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                                           const RtspResponse &response)
{
    LMRTSP_LOGD("ClientDescribeSentState: Received SETUP response (unexpected in DescribeSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientDescribeSentState::OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                                          const RtspResponse &response)
{
    LMRTSP_LOGD("ClientDescribeSentState: Received PLAY response (unexpected in DescribeSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientDescribeSentState::OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                                           const RtspResponse &response)
{
    LMRTSP_LOGD("ClientDescribeSentState: Received PAUSE response (unexpected in DescribeSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientDescribeSentState::OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                                              const RtspResponse &response)
{
    LMRTSP_LOGD("ClientDescribeSentState: Received TEARDOWN response");
    return ClientStateAction::SUCCESS;
}

// ClientSetupSentState implementation
ClientStateAction ClientSetupSentState::OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                                          const RtspResponse &response)
{
    LMRTSP_LOGD("ClientSetupSentState: Received OPTIONS response (unexpected in SetupSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientSetupSentState::OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                                           const RtspResponse &response)
{
    LMRTSP_LOGD("ClientSetupSentState: Received DESCRIBE response (unexpected in SetupSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientSetupSentState::OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                                        const RtspResponse &response)
{
    LMRTSP_LOGD("ClientSetupSentState: Received SETUP response");
    if (response.status_ == StatusCode::OK) {
        // SETUP succeeded, now send PLAY
        if (client && session) {
            std::string url = session->GetUrl();

            // Ensure URL ends with '/' for aggregate control
            if (!url.empty() && url.back() != '/') {
                url += '/';
            }

            std::string session_id = session->GetSessionId();
            if (client->SendPlayRequest(url, session_id)) {
                session->ChangeState(ClientPlaySentState::GetInstance());
                return ClientStateAction::CONTINUE;
            }
        }
        return ClientStateAction::FAIL;
    }
    return ClientStateAction::FAIL;
}

ClientStateAction ClientSetupSentState::OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                                       const RtspResponse &response)
{
    LMRTSP_LOGD("ClientSetupSentState: Received PLAY response (unexpected in SetupSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientSetupSentState::OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                                        const RtspResponse &response)
{
    LMRTSP_LOGD("ClientSetupSentState: Received PAUSE response (unexpected in SetupSent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientSetupSentState::OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                                           const RtspResponse &response)
{
    LMRTSP_LOGD("ClientSetupSentState: Received TEARDOWN response");
    return ClientStateAction::SUCCESS;
}

// ClientPlaySentState implementation
ClientStateAction ClientPlaySentState::OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                                         const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlaySentState: Received OPTIONS response (unexpected in PlaySent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientPlaySentState::OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                                          const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlaySentState: Received DESCRIBE response (unexpected in PlaySent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientPlaySentState::OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                                       const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlaySentState: Received SETUP response (unexpected in PlaySent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientPlaySentState::OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                                      const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlaySentState: Received PLAY response");
    if (response.status_ == StatusCode::OK) {
        // PLAY succeeded, transition to Playing state
        session->ChangeState(ClientPlayingState::GetInstance());
        session->SetState(RtspClientSessionState::PLAYING);
        // Handshake completed
        return ClientStateAction::SUCCESS;
    }
    return ClientStateAction::FAIL;
}

ClientStateAction ClientPlaySentState::OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                                       const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlaySentState: Received PAUSE response (unexpected in PlaySent state)");
    return ClientStateAction::FAIL;
}

ClientStateAction ClientPlaySentState::OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                                          const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlaySentState: Received TEARDOWN response");
    return ClientStateAction::SUCCESS;
}

// ClientPlayingState implementation
ClientStateAction ClientPlayingState::OnOptionsResponse(RtspClientSession *session, RtspClient *client,
                                                        const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlayingState: Received OPTIONS response");
    return ClientStateAction::WAIT;
}

ClientStateAction ClientPlayingState::OnDescribeResponse(RtspClientSession *session, RtspClient *client,
                                                         const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlayingState: Received DESCRIBE response");
    return ClientStateAction::WAIT;
}

ClientStateAction ClientPlayingState::OnSetupResponse(RtspClientSession *session, RtspClient *client,
                                                      const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlayingState: Received SETUP response");
    return ClientStateAction::WAIT;
}

ClientStateAction ClientPlayingState::OnPlayResponse(RtspClientSession *session, RtspClient *client,
                                                     const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlayingState: Received PLAY response");
    return ClientStateAction::WAIT;
}

ClientStateAction ClientPlayingState::OnPauseResponse(RtspClientSession *session, RtspClient *client,
                                                      const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlayingState: Received PAUSE response");
    return ClientStateAction::WAIT;
}

ClientStateAction ClientPlayingState::OnTeardownResponse(RtspClientSession *session, RtspClient *client,
                                                         const RtspResponse &response)
{
    LMRTSP_LOGD("ClientPlayingState: Received TEARDOWN response");
    return ClientStateAction::SUCCESS;
}

} // namespace lmshao::lmrtsp
