/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_server_session_state.h"

#include "internal_logger.h"
#include "lmrtsp/rtsp_server.h"
#include "lmrtsp/rtsp_server_session.h"
#include "rtsp_response.h"

namespace lmshao::lmrtsp {

namespace {

RtspResponse HandleOptions(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing OPTIONS request");
    int cseq = std::stoi(request.general_header_.at(CSEQ));
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
    int cseq = std::stoi(request.general_header_.at(CSEQ));
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
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse HandleSetParameter(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing SET_PARAMETER request");
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse HandleAnnounce(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing ANNOUNCE request");
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    return response;
}

RtspResponse HandleRecord(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing RECORD request");
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    return response;
}

} // namespace

// ServerInitialState implementations
RtspResponse ServerInitialState::OnOptionsRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse ServerInitialState::OnDescribeRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse ServerInitialState::OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse ServerInitialState::OnRecordRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse ServerInitialState::OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse ServerInitialState::OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse ServerInitialState::OnSetupRequest(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing SETUP request in InitialState");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

    if (session->SetupMedia(request.uri_, request.general_header_.at("Transport"))) {
        session->ChangeState(&ServerReadyState::GetInstance());
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

RtspResponse ServerInitialState::OnPlayRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ServerInitialState::OnPauseRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ServerInitialState::OnTeardownRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// ReadyState implementations
RtspResponse ServerReadyState::OnOptionsRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse ServerReadyState::OnDescribeRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse ServerReadyState::OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse ServerReadyState::OnRecordRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse ServerReadyState::OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse ServerReadyState::OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse ServerReadyState::OnSetupRequest(RtspServerSession *session, const RtspRequest &request)
{
    // Allow multiple SETUP requests for multi-track streams (e.g., MKV with video+audio)
    LMRTSP_LOGD("Processing additional SETUP request in ReadyState (multi-track support)");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

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

RtspResponse ServerReadyState::OnPlayRequest(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing PLAY request in ReadyState");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

    std::string range = "";
    if (request.requestHeader_.range_) {
        range = *request.requestHeader_.range_;
    }

    if (session->PlayMedia(request.uri_, range)) {
        session->ChangeState(&ServerPlayingState::GetInstance());
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

RtspResponse ServerReadyState::OnPauseRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ServerReadyState::OnTeardownRequest(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing TEARDOWN request in ReadyState");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

    session->TeardownMedia(request.uri_);
    session->ChangeState(&ServerInitialState::GetInstance());

    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// PlayingState implementations
RtspResponse ServerPlayingState::OnOptionsRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse ServerPlayingState::OnDescribeRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse ServerPlayingState::OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse ServerPlayingState::OnRecordRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse ServerPlayingState::OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse ServerPlayingState::OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse ServerPlayingState::OnSetupRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ServerPlayingState::OnPlayRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ServerPlayingState::OnPauseRequest(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing PAUSE request in PlayingState");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

    if (session->PauseMedia(request.uri_)) {
        session->ChangeState(&ServerPausedState::GetInstance());
        auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
        return response;
    } else {
        auto response = RtspResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();
        return response;
    }
}

RtspResponse ServerPlayingState::OnTeardownRequest(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing TEARDOWN request in PlayingState");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

    session->TeardownMedia(request.uri_);
    session->ChangeState(&ServerInitialState::GetInstance());

    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// PausedState implementations
RtspResponse ServerPausedState::OnOptionsRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleOptions(session, request);
}

RtspResponse ServerPausedState::OnDescribeRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleDescribe(session, request);
}

RtspResponse ServerPausedState::OnAnnounceRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleAnnounce(session, request);
}

RtspResponse ServerPausedState::OnRecordRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleRecord(session, request);
}

RtspResponse ServerPausedState::OnGetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleGetParameter(session, request);
}

RtspResponse ServerPausedState::OnSetParameterRequest(RtspServerSession *session, const RtspRequest &request)
{
    return HandleSetParameter(session, request);
}

RtspResponse ServerPausedState::OnSetupRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ServerPausedState::OnPlayRequest(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing PLAY request in PausedState");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

    std::string range = "";
    if (request.requestHeader_.range_) {
        range = *request.requestHeader_.range_;
    }

    if (session->PlayMedia(request.uri_, range)) {
        session->ChangeState(&ServerPlayingState::GetInstance());
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

RtspResponse ServerPausedState::OnPauseRequest(RtspServerSession *session, const RtspRequest &request)
{
    int cseq = std::stoi(request.general_header_.at(CSEQ));
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

RtspResponse ServerPausedState::OnTeardownRequest(RtspServerSession *session, const RtspRequest &request)
{
    LMRTSP_LOGD("Processing TEARDOWN request in PausedState");
    int cseq = std::stoi(request.general_header_.at(CSEQ));

    session->TeardownMedia(request.uri_);
    session->ChangeState(&ServerInitialState::GetInstance());

    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();
    return response;
}

// RtspServerSessionState is an abstract base class - no default implementations needed

// GetName functions are implemented inline in the header file

} // namespace lmshao::lmrtsp