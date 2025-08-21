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

#include "media_stream.h"
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
        {METHOD_ANNOUNCE, &RTSPSessionState::OnAnnounce},
        {METHOD_RECORD, &RTSPSessionState::OnRecord},
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
    RTSP_LOGD("Setting up media for track: %s", uri.c_str());

    // 1. Extract trackID from URI
    size_t last_slash = uri.find_last_of('/');
    if (last_slash == std::string::npos) {
        RTSP_LOGE("Invalid track URI: %s", uri.c_str());
        return false;
    }
    std::string track_id = uri.substr(last_slash + 1);

    // 2. Check if media stream for this track already exists
    std::shared_ptr<MediaStream> media_stream;
    for (const auto& stream : mediaStreams_) {
        // The URI of a media stream is its track id
        if (stream->GetUri() == track_id) {
            media_stream = stream;
            break;
        }
    }

    if (!media_stream) {
        // 3. If not, create a new one
        RTSP_LOGD("Creating new media stream for track: %s", track_id.c_str());
        // TODO: get mediaType from SDP
        media_stream = MediaStreamFactory::CreateStream(track_id, "video");
        if (media_stream) {
            media_stream->SetSession(weak_from_this());
            media_stream->SetTrackIndex(mediaStreams_.size());
            mediaStreams_.push_back(media_stream);
        } else {
            RTSP_LOGE("Failed to create media stream for track: %s", track_id.c_str());
            return false;
        }
    }

    // 4. Setup the stream
    if (!media_stream->Setup(transport, networkSession_->host)) {
        RTSP_LOGE("Failed to setup media stream for track: %s", track_id.c_str());
        return false;
    }

    // 5. Construct Transport header for response
    transportInfo_ = media_stream->GetTransportInfo();

    return true;
}

bool RTSPSession::PlayMedia(const std::string &uri, const std::string &range)
{
    RTSP_LOGD("Playing media for URI: %s", uri.c_str());

    // If URI is the aggregate URI (without track ID), play all streams
    size_t last_slash = uri.find_last_of('/');
    if (last_slash == std::string::npos || last_slash == uri.length() - 1) {
        bool success = true;
        for (auto &stream : mediaStreams_) {
            if (!stream->Play(range)) {
                RTSP_LOGE("Failed to play media stream: %s", stream->GetUri().c_str());
                success = false;
            }
        }
        return success;
    }

    // Otherwise, play the specific track
    std::string track_id = uri.substr(last_slash + 1);
    for (auto &stream : mediaStreams_) {
        if (stream->GetUri() == track_id) {
            return stream->Play(range);
        }
    }

    RTSP_LOGE("Media stream not found for track: %s", track_id.c_str());
    return false;
}

bool RTSPSession::PauseMedia(const std::string &uri)
{
    RTSP_LOGD("Pausing media for URI: %s", uri.c_str());

    // If URI is the aggregate URI (without track ID), pause all streams
    size_t last_slash = uri.find_last_of('/');
    if (last_slash == std::string::npos || last_slash == uri.length() - 1) {
        bool success = true;
        for (auto &stream : mediaStreams_) {
            if (!stream->Pause()) {
                RTSP_LOGE("Failed to pause media stream: %s", stream->GetUri().c_str());
                success = false;
            }
        }
        return success;
    }

    // Otherwise, pause the specific track
    std::string track_id = uri.substr(last_slash + 1);
    for (auto &stream : mediaStreams_) {
        if (stream->GetUri() == track_id) {
            return stream->Pause();
        }
    }

    RTSP_LOGE("Media stream not found for track: %s", track_id.c_str());
    return false;
}

bool RTSPSession::TeardownMedia(const std::string &uri)
{
    RTSP_LOGD("Tearing down media for URI: %s", uri.c_str());

    // If URI is the aggregate URI (without track ID), teardown all streams
    size_t last_slash = uri.find_last_of('/');
    if (last_slash == std::string::npos || last_slash == uri.length() - 1) {
        bool success = true;
        for (auto &stream : mediaStreams_) {
            if (!stream->Teardown()) {
                RTSP_LOGE("Failed to teardown media stream: %s", stream->GetUri().c_str());
                success = false;
            }
        }
        mediaStreams_.clear();
        return success;
    }

    // Otherwise, teardown the specific track
    std::string track_id = uri.substr(last_slash + 1);
    for (auto it = mediaStreams_.begin(); it != mediaStreams_.end(); ++it) {
        if ((*it)->GetUri() == track_id) {
            bool success = (*it)->Teardown();
            if (success) {
                mediaStreams_.erase(it);
            }
            return success;
        }
    }

    RTSP_LOGE("Media stream not found for track: %s", track_id.c_str());
    return false;
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

std::shared_ptr<MediaStream> RTSPSession::GetMediaStream(int track_index)
{
    if (track_index < 0 || static_cast<size_t>(track_index) >= mediaStreams_.size()) {
        return nullptr;
    }
    return mediaStreams_[track_index];
}

const std::vector<std::shared_ptr<MediaStream>> &RTSPSession::GetMediaStreams() const
{
    return mediaStreams_;
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