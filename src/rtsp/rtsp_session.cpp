/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_session.h"

#include <ctime>
#include <functional>
#include <random>
#include <unordered_map>

#include "rtsp/media_stream.h"
#include "rtsp/rtsp_session_state.h"
#include "rtsp_log.h"

namespace lmshao::rtsp {

RTSPSession::RTSPSession(std::shared_ptr<network::Session> networkSession)
    : networkSession_(networkSession), timeout_(60)
{ // Default 60 seconds timeout

    // Generate session ID
    sessionId_ = GenerateSessionId();

    // Set initial state
    currentState_ = InitialState::GetInstance();

    // Record current time
    lastActiveTime_ = std::time(nullptr);

    RTSP_LOGD("Created RTSP session: %s", sessionId_.c_str());
}

RTSPSession::~RTSPSession()
{
    // Clean up all media streams
    for (auto &stream : mediaStreams_) {
        stream->Teardown();
    }
    mediaStreams_.clear();

    RTSP_LOGD("Destroyed RTSP session: %s", sessionId_.c_str());
}

RTSPResponse RTSPSession::ProcessRequest(const RTSPRequest &request)
{
    // Update last active time
    lastActiveTime_ = std::time(nullptr);

    RTSP_LOGD("Processing %s request in state %s", request.method_.c_str(), currentState_->GetName().c_str());

    // Method to state handler mapping - O(1) lookup instead of O(n) if-else chain
    using MethodHandler = RTSPResponse (RTSPSessionState::*)(RTSPSession *, const RTSPRequest &);

    static const std::unordered_map<std::string, MethodHandler> method_handlers = {
        {METHOD_OPTIONS, &RTSPSessionState::OnOptions},
        {METHOD_DESCRIBE, &RTSPSessionState::OnDescribe},
        {METHOD_SETUP, &RTSPSessionState::OnSetup},
        {METHOD_PLAY, &RTSPSessionState::OnPlay},
        {METHOD_PAUSE, &RTSPSessionState::OnPause},
        {METHOD_TEARDOWN, &RTSPSessionState::OnTeardown},
        {METHOD_GET_PARAMETER, &RTSPSessionState::OnGetParameter},
        {METHOD_SET_PARAMETER, &RTSPSessionState::OnSetParameter}};

    auto it = method_handlers.find(request.method_);
    if (it != method_handlers.end()) {
        return (currentState_.get()->*(it->second))(this, request);
    }

    // Unsupported method
    RTSP_LOGW("Unsupported RTSP method: %s", request.method_.c_str());

    // Build error response
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::NotImplemented)
                        .SetCSeq(std::stoi(request.general_header_.at(CSEQ)))
                        .Build();

    return response;
}

void RTSPSession::ChangeState(std::shared_ptr<RTSPSessionState> newState)
{
    RTSP_LOGD("Changing state from %s to %s", currentState_->GetName().c_str(), newState->GetName().c_str());

    currentState_ = newState;
}

std::shared_ptr<RTSPSessionState> RTSPSession::GetCurrentState() const
{
    return currentState_;
}

std::string RTSPSession::GetSessionId() const
{
    return sessionId_;
}

std::shared_ptr<network::Session> RTSPSession::GetNetworkSession() const
{
    return networkSession_;
}

bool RTSPSession::SetupMedia(const std::string &uri, const std::string &transport)
{
    RTSP_LOGD("Setting up media for URI: %s, transport: %s", uri.c_str(), transport.c_str());

    // Parse media type
    std::string mediaType = "video"; // Default to video
    if (uri.find("/audio/") != std::string::npos) {
        mediaType = "audio";
    }

    // Create media stream
    auto stream = MediaStreamFactory::CreateStream(uri, mediaType);
    if (!stream) {
        RTSP_LOGE("Failed to create media stream for URI: %s", uri.c_str());
        return false;
    }

    // Set transport parameters
    if (!stream->Setup(transport)) {
        RTSP_LOGE("Failed to setup media stream: %s", uri.c_str());
        return false;
    }

    // Save transport information
    transportInfo_ = stream->GetTransportInfo();

    // Add to media stream list
    mediaStreams_.push_back(stream);

    RTSP_LOGD("Media setup successful for URI: %s", uri.c_str());
    return true;
}

bool RTSPSession::PlayMedia(const std::string &range)
{
    RTSP_LOGD("Playing media, range: %s", range.c_str());

    bool success = true;
    for (auto &stream : mediaStreams_) {
        if (!stream->Play(range)) {
            RTSP_LOGE("Failed to play media stream");
            success = false;
        }
    }

    return success;
}

bool RTSPSession::PauseMedia()
{
    RTSP_LOGD("Pausing media");

    bool success = true;
    for (auto &stream : mediaStreams_) {
        if (!stream->Pause()) {
            RTSP_LOGE("Failed to pause media stream");
            success = false;
        }
    }

    return success;
}

bool RTSPSession::TeardownMedia()
{
    RTSP_LOGD("Tearing down media");

    bool success = true;
    for (auto &stream : mediaStreams_) {
        if (!stream->Teardown()) {
            RTSP_LOGE("Failed to teardown media stream");
            success = false;
        }
    }

    // Clear media stream list
    mediaStreams_.clear();

    return success;
}

void RTSPSession::SetSdpDescription(const std::string &sdp)
{
    sdpDescription_ = sdp;
}

std::string RTSPSession::GetSdpDescription() const
{
    return sdpDescription_;
}

void RTSPSession::SetTransportInfo(const std::string &transport)
{
    transportInfo_ = transport;
}

std::string RTSPSession::GetTransportInfo() const
{
    return transportInfo_;
}

std::string RTSPSession::GenerateSessionId()
{
    thread_local static std::random_device rd;
    uint32_t part1 = rd();
    uint32_t part2 = rd();

    char buffer[18];
    snprintf(buffer, sizeof(buffer), "%08X%08X", part1, part2);
    return buffer;
}

} // namespace lmshao::rtsp