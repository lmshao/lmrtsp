/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_session_state.h"

#include "internal_logger.h"
#include "lmrtsp/rtsp_server.h"
#include "lmrtsp/rtsp_server_session.h"
#include "rtsp_response.h"

namespace lmshao::lmrtsp {

namespace {

RtspResponse HandleOptions(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing OPTIONS request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetPublic("OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE")
                        .Build();
    return response;
}

RtspResponse HandleDescribe(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing DESCRIBE request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    RtspResponseBuilder builder;

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

RtspResponse HandleGetParameter(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing GET_PARAMETER request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse HandleSetParameter(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing SET_PARAMETER request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse HandleAnnounce(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing ANNOUNCE request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    return response;
}

RtspResponse HandleRecord(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing RECORD request");
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    return response;
}

} // namespace

// InitialState implementations
RtspResponse InitialState::OnOptions(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse InitialState::OnDescribe(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse InitialState::OnAnnounce(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse InitialState::OnRecord(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse InitialState::OnGetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse InitialState::OnSetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse InitialState::OnSetup(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing SETUP request in InitialState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    if (session->SetupMedia(request.uri_, request.general_header_.at("Transport"))) {
        session->ChangeState(ReadyState::GetInstance());
        auto response = RtspResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(cseq)
                            .SetSession(session->GetSessionId())
                            .SetTransport(session->GetTransportInfo())
                            .Build();
        return response;
    } else {
        auto response = RtspResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RtspResponse InitialState::OnPlay(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse InitialState::OnPause(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse InitialState::OnTeardown(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// ReadyState implementations
RtspResponse ReadyState::OnOptions(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse ReadyState::OnDescribe(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse ReadyState::OnAnnounce(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse ReadyState::OnRecord(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse ReadyState::OnGetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse ReadyState::OnSetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse ReadyState::OnSetup(RtspServerSession *session, const RtspRequest &request)
{
    // Allow multiple SETUP requests for multi-track streams (e.g., MKV with video+audio)
    LMRTSP_LOGD("Processing additional SETUP request in ReadyState (multi-track support)");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // Process the SETUP request (for additional tracks)
    if (session->SetupMedia(request.uri_, request.general_header_.at("Transport"))) {
        // Stay in ReadyState (already setup)
        auto response = RtspResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(cseq)
                            .SetSession(session->GetSessionId())
                            .SetTransport(session->GetTransportInfo())
                            .Build();
        return response;
    } else {
        auto response = RtspResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RtspResponse ReadyState::OnPlay(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing PLAY request in ReadyState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    std::string range = "";
    if (request.requestHeader_.range_) {
        range = *request.requestHeader_.range_;
    }

    if (session->PlayMedia(request.uri_, range)) {
        session->ChangeState(PlayingState::GetInstance());
        auto response = RtspResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(cseq)
                            .SetSession(session->GetSessionId())
                            .SetRange(range.empty() ? "npt=0-" : range)
                            .SetRTPInfo("url=" + session->GetStreamUri() + ";" + session->GetRtpInfo())
                            .Build();
        return response;
    } else {
        auto response = RtspResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RtspResponse ReadyState::OnPause(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ReadyState::OnTeardown(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing TEARDOWN request in ReadyState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    session->TeardownMedia(request.uri_);
    session->ChangeState(InitialState::GetInstance());

    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// PlayingState implementations
RtspResponse PlayingState::OnOptions(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse PlayingState::OnDescribe(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse PlayingState::OnAnnounce(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse PlayingState::OnRecord(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse PlayingState::OnGetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse PlayingState::OnSetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse PlayingState::OnSetup(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse PlayingState::OnPlay(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse PlayingState::OnPause(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing PAUSE request in PlayingState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    if (session->PauseMedia(request.uri_)) {
        session->ChangeState(PausedState::GetInstance());
        auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
        return response;
    } else {
        auto response = RtspResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RtspResponse PlayingState::OnTeardown(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing TEARDOWN request in PlayingState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    session->TeardownMedia(request.uri_);
    session->ChangeState(InitialState::GetInstance());

    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// PausedState implementations
RtspResponse PausedState::OnOptions(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse PausedState::OnDescribe(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse PausedState::OnAnnounce(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse PausedState::OnRecord(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse PausedState::OnGetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse PausedState::OnSetParameter(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse PausedState::OnSetup(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse PausedState::OnPlay(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing PLAY request in PausedState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    std::string range = "";
    if (request.requestHeader_.range_) {
        range = *request.requestHeader_.range_;
    }

    if (session->PlayMedia(request.uri_, range)) {
        session->ChangeState(PlayingState::GetInstance());
        auto response = RtspResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(cseq)
                            .SetSession(session->GetSessionId())
                            .SetRange(range.empty() ? "npt=0-" : range)
                            .SetRTPInfo("url=" + session->GetStreamUri() + ";" + session->GetRtpInfo())
                            .Build();
        return response;
    } else {
        auto response = RtspResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RtspResponse PausedState::OnPause(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at("CSeq"));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse PausedState::OnTeardown(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing TEARDOWN request in PausedState");
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    session->TeardownMedia(request.uri_);
    session->ChangeState(InitialState::GetInstance());

    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// RtspServerSessionState is an abstract base class - no default implementations needed

// GetName functions are implemented inline in the header file

} // namespace lmshao::lmrtsp