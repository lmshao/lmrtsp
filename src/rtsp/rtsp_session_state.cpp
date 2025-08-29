/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_session_state.h"

#include "rtsp_log.h"
#include "rtsp_session.h"

namespace lmshao::rtsp {

// Common RTSP method handling functions that can be reused by various state classes
namespace {

RTSPResponse HandleOptions(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing OPTIONS request");

    // OPTIONS requests can be handled in all states
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Build response
    auto response =
        RTSPResponseBuilder()
            .SetStatus(StatusCode::OK)
            .SetCSeq(cseq)
            .SetPublic(
                "OPTIONS, DESCRIBE, ANNOUNCE, RECORD, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER, SET_PARAMETER")
            .Build();

    return response;
}

RTSPResponse HandleDescribe(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing DESCRIBE request");

    // DESCRIBE requests can be handled in all states
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Get request URI
    std::string uri = request.uri_;

    // Here we should get media information based on URI and generate SDP description
    // Simplified implementation, using an example SDP
    std::string sdp = "v=0\r\n"
                      "o=- 12345 12345 IN IP4 127.0.0.1\r\n"
                      "s=RTSP Session\r\n"
                      "t=0 0\r\n"
                      "m=video 0 RTP/AVP 96\r\n"
                      "a=rtpmap:96 H264/90000\r\n"
                      "a=control:track1\r\n"
                      "m=audio 0 RTP/AVP 97\r\n"
                      "a=rtpmap:97 MPEG4-GENERIC/44100/2\r\n"
                      "a=control:track2\r\n";

    // Save SDP description to session
    session->SetSdpDescription(sdp);

    // Build response
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetContentType("application/sdp")
                        .SetContentLength(sdp.size())
                        .SetMessageBody(sdp)
                        .Build();

    return response;
}

RTSPResponse HandleGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing GET_PARAMETER request");

    // GET_PARAMETER requests can be handled in all states
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Build response
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse HandleSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SET_PARAMETER request");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Simplified implementation, return success response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse HandleAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing ANNOUNCE request");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // here we can get sdp info from request.message_body_

    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse HandleRecord(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing RECORD request");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // here we can get sdp info from request.message_body_

    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

} // namespace

RTSPResponse InitialState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
}

RTSPResponse ReadyState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
}

RTSPResponse PlayingState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
}

RTSPResponse PausedState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
}

RTSPResponse InitialState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse InitialState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse InitialState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse InitialState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse InitialState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse InitialState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SETUP request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Get Transport header
    std::string transport = "";
    if (request.general_header_.find(TRANSPORT) != request.general_header_.end()) {
        transport = request.general_header_.at(TRANSPORT);
    }
    RTSP_LOGD("Transport: %s", transport.c_str());

    // Setup media
    if (!session->SetupMedia(request.uri_, transport)) {
        RTSP_LOGE("Failed to setup media");

        // Build error response
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::NotFound).SetCSeq(cseq).Build();

        return response;
    }

    // Switch to Ready state
    session->ChangeState(std::static_pointer_cast<RTSPSessionState>(ReadyState::GetInstance()));

    // Build success response
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetSession(session->GetSessionId())
                        .SetTransport(session->GetTransportInfo())
                        .Build();

    return response;
}

RTSPResponse InitialState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Cannot execute PLAY in Initial state
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse InitialState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Cannot execute PAUSE in Initial state
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse InitialState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Cannot execute TEARDOWN in Initial state
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse ReadyState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse ReadyState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse ReadyState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse ReadyState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse ReadyState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse PlayingState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse ReadyState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    // In Ready state, can continue to setup other media tracks
    return InitialState::GetInstance()->OnSetup(session, request);
}

RTSPResponse ReadyState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in ReadyState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Get Range header (if any)
    std::string range = "";
    if (request.request_header_.range_) {
        range = *request.request_header_.range_;
    }

    // Start playing
    if (!session->PlayMedia(request.uri_, range)) {
        RTSP_LOGE("Failed to start playing");

        // Build error response
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();

        return response;
    }

    // Switch to Playing state
    session->ChangeState(std::static_pointer_cast<RTSPSessionState>(PlayingState::GetInstance()));

    // Build success response
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetSession(session->GetSessionId())
                        .SetRange(range.empty() ? "npt=0-" : range)
                        // Here we should add RTP-Info header with RTP timestamp information
                        .SetRTPInfo("url=rtsp://example.com/track1;seq=1000;rtptime=123456")
                        .Build();

    return response;
}

RTSPResponse ReadyState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in ReadyState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Cannot execute PAUSE in Ready state
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse ReadyState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in ReadyState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Release media resources
    session->TeardownMedia(request.uri_);

    // Switch to Initial state
    session->ChangeState(InitialState::GetInstance());

    // Build response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse PlayingState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse PlayingState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse PlayingState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse PlayingState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse PlayingState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SETUP request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Cannot execute SETUP in Playing state
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse PlayingState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Already playing, return OK
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse PlayingState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Pause playing
    if (!session->PauseMedia(request.uri_)) {
        RTSP_LOGE("Failed to pause playing");

        // Build error response
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();

        return response;
    }

    // Switch to Paused state
    session->ChangeState(PausedState::GetInstance());

    // Build success response
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse PlayingState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Release media resources
    session->TeardownMedia(request.uri_);

    // Switch to Initial state
    session->ChangeState(InitialState::GetInstance());

    // Build response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

// PausedState implementation

// GetName method is already defined in header file

RTSPResponse PausedState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse PausedState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse PausedState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse PausedState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse PausedState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse PausedState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SETUP request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Cannot execute SETUP in Paused state
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse PausedState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Get Range header (if any)
    std::string range = "";
    if (request.request_header_.range_) {
        range = *request.request_header_.range_;
    }

    // Resume playing
    if (!session->PlayMedia(request.uri_, range)) {
        RTSP_LOGE("Failed to resume playing");

        // Build error response
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();

        return response;
    }

    // Switch to Playing state
    session->ChangeState(PlayingState::GetInstance());

    // Build success response
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetSession(session->GetSessionId())
                        .SetRange(range.empty() ? "npt=0-" : range)
                        // Here we should add RTP-Info header with RTP timestamp information
                        .SetRTPInfo("url=rtsp://example.com/track1;seq=1000;rtptime=123456")
                        .Build();

    return response;
}

RTSPResponse PausedState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Already paused, return OK
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse PausedState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Release media resources
    session->TeardownMedia(request.uri_);

    // Switch to Initial state
    session->ChangeState(InitialState::GetInstance());

    // Build response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("SETUP not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Build error response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("PLAY not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Build error response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("PAUSE not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Build error response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("TEARDOWN not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Build error response
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

} // namespace lmshao::rtsp