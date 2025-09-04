/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_session_state.h"

#include "rtsp_log.h"
#include "rtsp_response.h"
#include "rtsp_server.h"
#include "rtsp_session.h"

namespace lmshao::rtsp {

// Common RTSP method handling functions
namespace {

RTSPResponse HandleOptions(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing OPTIONS request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetPublic("OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE")
                        .Build();
    return response;
}

RTSPResponse HandleDescribe(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing DESCRIBE request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    RTSPResponseBuilder builder;

    // Get server reference from session
    auto server = session->GetRTSPServer();
    if (auto serverPtr = server.lock()) {
        // Extract stream name from URI
        std::string uri = request.uri_;
        std::string streamName = uri.substr(uri.find_last_of('/') + 1);

        // Check if stream exists using GetMediaStream
        auto streamInfo = serverPtr->GetMediaStream(streamName);
        if (streamInfo) {
            // Generate SDP description for the stream
            std::string sdp = serverPtr->GenerateSDP(streamName, serverPtr->GetServerIP(), serverPtr->GetServerPort());
            session->SetSdpDescription(sdp);

            return builder.SetStatus(StatusCode::OK)
                .SetCSeq(cseq)
                .SetContentType("application/sdp")
                .SetSdp(sdp)
                .Build();
        } else {
            // Stream not found
            return builder.SetStatus(StatusCode::NotFound).SetCSeq(cseq).Build();
        }
    } else {
        // Server reference is invalid
        return builder.SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
    }
}

RTSPResponse HandleGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing GET_PARAMETER request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse HandleSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SET_PARAMETER request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse HandleAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing ANNOUNCE request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse HandleRecord(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing RECORD request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    return response;
}

} // namespace

// InitialState implementations
RTSPResponse InitialState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse InitialState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse InitialState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse InitialState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
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

    if (session->SetupMedia(request.uri_, request.general_header_.at("Transport"))) {
        session->ChangeState(ReadyState::GetInstance());
        auto response = RTSPResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(cseq)
                            .SetSession(session->GetSessionId())
                            .SetTransport(session->GetTransportInfo())
                            .Build();
        return response;
    } else {
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RTSPResponse InitialState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse InitialState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse InitialState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// ReadyState implementations
RTSPResponse ReadyState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse ReadyState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse ReadyState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse ReadyState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
}

RTSPResponse ReadyState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse ReadyState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse ReadyState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse ReadyState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in ReadyState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    std::string range = "";
    if (request.request_header_.range_) {
        range = *request.request_header_.range_;
    }

    if (session->PlayMedia(request.uri_, range)) {
        session->ChangeState(PlayingState::GetInstance());
        auto response = RTSPResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(cseq)
                            .SetSession(session->GetSessionId())
                            .SetRange(range.empty() ? "npt=0-" : range)
                            .Build();
        return response;
    } else {
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RTSPResponse ReadyState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse ReadyState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in ReadyState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    session->TeardownMedia(request.uri_);
    session->ChangeState(InitialState::GetInstance());

    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// PlayingState implementations
RTSPResponse PlayingState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse PlayingState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse PlayingState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse PlayingState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
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
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse PlayingState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse PlayingState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in PlayingState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    if (session->PauseMedia(request.uri_)) {
        session->ChangeState(PausedState::GetInstance());
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
        return response;
    } else {
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RTSPResponse PlayingState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in PlayingState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    session->TeardownMedia(request.uri_);
    session->ChangeState(InitialState::GetInstance());

    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// PausedState implementations
RTSPResponse PausedState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse PausedState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse PausedState::OnAnnounce(RTSPSession *session, const RTSPRequest &request)
{
    return HandleAnnounce(session, request);
}

RTSPResponse PausedState::OnRecord(RTSPSession *session, const RTSPRequest &request)
{
    return HandleRecord(session, request);
}

RTSPResponse PausedState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse PausedState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse PausedState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse PausedState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in PausedState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    std::string range = "";
    if (request.request_header_.range_) {
        range = *request.request_header_.range_;
    }

    if (session->PlayMedia(request.uri_, range)) {
        session->ChangeState(PlayingState::GetInstance());
        auto response = RTSPResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(cseq)
                            .SetSession(session->GetSessionId())
                            .SetRange(range.empty() ? "npt=0-" : range)
                            .Build();
        return response;
    } else {
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RTSPResponse PausedState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RTSPResponse PausedState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in PausedState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    session->TeardownMedia(request.uri_);
    session->ChangeState(InitialState::GetInstance());

    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// RTSPSessionState is an abstract base class - no default implementations needed

// GetName functions are implemented inline in the header file

} // namespace lmshao::rtsp