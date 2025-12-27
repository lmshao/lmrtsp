/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_server_session.h"

#include <lmcore/time_utils.h>
#include <lmcore/uuid.h>

#include <string>

#include "internal_logger.h"
#include "lmrtsp/rtsp_media_stream_manager.h"
#include "lmrtsp/rtsp_server.h"
#include "rtsp_response.h"
#include "rtsp_server_session_state.h"

namespace lmshao::lmrtsp {

RtspServerSession::RtspServerSession(std::shared_ptr<lmnet::Session> lmnetSession)
    : lmnetSession_(lmnetSession), timeout_(60),
      mediaStreamManager_(std::make_unique<lmshao::lmrtsp::RtspMediaStreamManager>(std::weak_ptr<RtspServerSession>()))
{ // Default 60 seconds timeout

    // Generate session ID
    sessionId_ = GenerateSessionId();

    // Initialize last active time
    lastActiveTime_ = lmcore::TimeUtils::GetCurrentTimeMs();

    // Initialize state machine to Initial state
    currentState_ = &ServerInitialState::GetInstance();

    LMRTSP_LOGD("RtspServerSession created with ID: %s", sessionId_.c_str());
}

RtspServerSession::RtspServerSession(std::shared_ptr<lmnet::Session> lmnetSession, std::weak_ptr<RtspServer> server)
    : lmnetSession_(lmnetSession), rtspServer_(server), timeout_(60),
      mediaStreamManager_(std::make_unique<lmshao::lmrtsp::RtspMediaStreamManager>(std::weak_ptr<RtspServerSession>()))
{
    // Generate session ID
    sessionId_ = GenerateSessionId();

    // Initialize last active time
    lastActiveTime_ = lmcore::TimeUtils::GetCurrentTimeMs();

    // Initialize state machine to Initial state
    currentState_ = &ServerInitialState::GetInstance();

    LMRTSP_LOGD("RtspServerSession created with ID: %s and server reference", sessionId_.c_str());
}

RtspServerSession::~RtspServerSession()
{
    LMRTSP_LOGD("RtspServerSession destroyed: %s", sessionId_.c_str());

    // Clean up media streams
    mediaStreams_.clear();
}

RtspResponse RtspServerSession::ProcessRequest(const RtspRequest &request)
{
    // Update last active time
    UpdateLastActiveTime();

    // Use state machine to process request
    if (!currentState_) {
        // Fallback: initialize to Initial state if not set
        currentState_ = &ServerInitialState::GetInstance();
    }

    const std::string &method = request.method_;

    // Delegate to state machine based on method
    if (method == "OPTIONS") {
        return currentState_->OnOptionsRequest(this, request);
    } else if (method == "DESCRIBE") {
        return currentState_->OnDescribeRequest(this, request);
    } else if (method == "ANNOUNCE") {
        return currentState_->OnAnnounceRequest(this, request);
    } else if (method == "RECORD") {
        return currentState_->OnRecordRequest(this, request);
    } else if (method == "SETUP") {
        return currentState_->OnSetupRequest(this, request);
    } else if (method == "PLAY") {
        return currentState_->OnPlayRequest(this, request);
    } else if (method == "PAUSE") {
        return currentState_->OnPauseRequest(this, request);
    } else if (method == "TEARDOWN") {
        return currentState_->OnTeardownRequest(this, request);
    } else if (method == "GET_PARAMETER") {
        return currentState_->OnGetParameterRequest(this, request);
    } else if (method == "SET_PARAMETER") {
        return currentState_->OnSetParameterRequest(this, request);
    } else {
        // Unknown method
        int cseq = 0;
        auto cseq_it = request.general_header_.find(CSEQ);
        if (cseq_it != request.general_header_.end()) {
            cseq = std::stoi(cseq_it->second);
        }
        return RtspResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    }
}

void RtspServerSession::ChangeState(RtspServerSessionState *newState)
{
    currentState_ = newState;
}

RtspServerSessionState *RtspServerSession::GetCurrentState() const
{
    return currentState_;
}

std::string RtspServerSession::GetSessionId() const
{
    return sessionId_;
}

std::string RtspServerSession::GetClientIP() const
{
    if (lmnetSession_) {
        return lmnetSession_->host;
    }
    return "";
}

uint16_t RtspServerSession::GetClientPort() const
{
    if (lmnetSession_) {
        return lmnetSession_->port;
    }
    return 0;
}

std::shared_ptr<lmnet::Session> RtspServerSession::GetNetworkSession() const
{
    return lmnetSession_;
}

std::weak_ptr<RtspServer> RtspServerSession::GetRTSPServer() const
{
    return rtspServer_;
}

bool RtspServerSession::SetupMedia(const std::string &uri, const std::string &transport)
{
    LMRTSP_LOGD("Setting up media for URI: %s, Transport: %s", uri.c_str(), transport.c_str());

    // Extract track index from URI (e.g., /file.mkv/track0 -> 0)
    int track_index = -1;
    size_t track_pos = uri.rfind("/track");
    if (track_pos != std::string::npos) {
        std::string track_num_str;
        for (size_t i = track_pos + 6; i < uri.length(); ++i) {
            if (!std::isdigit(uri[i]))
                break;
            track_num_str += uri[i];
        }
        if (!track_num_str.empty()) {
            track_index = std::stoi(track_num_str);
            LMRTSP_LOGD("Detected multi-track SETUP: track index = %d", track_index);
        }
    }

    // Create transport config from parsed parameters
    lmshao::lmrtsp::TransportConfig transportConfig;

    // Determine transport type
    if (transport.find("RTP/AVP/TCP") != std::string::npos) {
        transportConfig.type = lmshao::lmrtsp::TransportConfig::Type::TCP_INTERLEAVED;
        // Extract interleaved channels
        size_t interleavedPos = transport.find("interleaved=");
        if (interleavedPos != std::string::npos) {
            std::string channelStr = transport.substr(interleavedPos + 12);
            size_t dashPos = channelStr.find('-');
            if (dashPos != std::string::npos) {
                transportConfig.rtpChannel = std::stoi(channelStr.substr(0, dashPos));
                transportConfig.rtcpChannel = std::stoi(channelStr.substr(dashPos + 1));
                LMRTSP_LOGD("Parsed interleaved channels: rtp=%d, rtcp=%d", transportConfig.rtpChannel,
                            transportConfig.rtcpChannel);
            }
        }
    } else {
        transportConfig.type = lmshao::lmrtsp::TransportConfig::Type::UDP;
        transportConfig.client_ip = GetClientIP();
        transportConfig.mode = lmshao::lmrtsp::TransportConfig::Mode::SOURCE;

        // Parse client_port parameter
        size_t clientPortPos = transport.find("client_port=");
        if (clientPortPos != std::string::npos) {
            std::string portStr = transport.substr(clientPortPos + 12);
            size_t dashPos = portStr.find('-');
            size_t semicolonPos = portStr.find(';');

            if (dashPos != std::string::npos) {
                std::string rtpPortStr = portStr.substr(0, dashPos);
                std::string rtcpPortStr = portStr.substr(
                    dashPos + 1, semicolonPos != std::string::npos ? semicolonPos - dashPos - 1 : std::string::npos);

                try {
                    transportConfig.client_rtp_port = std::stoi(rtpPortStr);
                    transportConfig.client_rtcp_port = std::stoi(rtcpPortStr);
                    LMRTSP_LOGD("Parsed client ports: RTP=%u, RTCP=%u", transportConfig.client_rtp_port,
                                transportConfig.client_rtcp_port);
                } catch (...) {
                    LMRTSP_LOGW("Failed to parse client port numbers");
                }
            }
        } else {
            LMRTSP_LOGW("No client_port found in Transport header; RTCP over UDP will be disabled");
        }

        // Server ports will be allocated dynamically (set to 0)
        transportConfig.server_rtp_port = 0;
        transportConfig.server_rtcp_port = 0;
    }

    // Multi-track or single-track setup
    if (track_index >= 0) {
        // Multi-track setup: create a separate stream manager for this track
        std::lock_guard<std::mutex> lock(tracksMutex_);

        InternalTrackInfo track_info;
        track_info.uri = uri;
        track_info.track_index = track_index;
        track_info.stream_info = mediaStreamInfo_; // Set by HandleRequest before SetupMedia

        // Create stream manager for this track
        track_info.stream_manager = std::make_unique<lmshao::lmrtsp::RtspMediaStreamManager>(
            std::weak_ptr<RtspServerSession>(shared_from_this()));

        if (!track_info.stream_manager->Setup(transportConfig)) {
            LMRTSP_LOGE("Failed to setup stream manager for track %d", track_index);
            return false;
        }

        track_info.transport_info = track_info.stream_manager->GetTransportInfo();
        tracks_[track_index] = std::move(track_info);

        // Update transportInfo_ with the latest track's transport (for SETUP response)
        transportInfo_ = tracks_[track_index].transport_info;

        LMRTSP_LOGD("Multi-track setup completed: track %d, Transport: %s", track_index, transportInfo_.c_str());
    } else {
        // Single-track setup (legacy mode)
        std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);

        mediaStreamManager_ = std::make_unique<lmshao::lmrtsp::RtspMediaStreamManager>(
            std::weak_ptr<RtspServerSession>(shared_from_this()));

        if (!mediaStreamManager_->Setup(transportConfig)) {
            LMRTSP_LOGE("Failed to setup media stream manager");
            mediaStreamManager_.reset();
            return false;
        }

        transportInfo_ = mediaStreamManager_->GetTransportInfo();
        streamUri_ = uri;

        LMRTSP_LOGD("Single-track setup completed, Transport: %s", transportInfo_.c_str());
    }

    // Set state to READY
    SetState(ServerSessionStateEnum::READY);

    LMRTSP_LOGD("Media setup completed for session: %s", sessionId_.c_str());
    return true;
}

bool RtspServerSession::PlayMedia(const std::string &uri, const std::string &range)
{
    LMRTSP_LOGD("Playing media for URI: %s, Range: %s", uri.c_str(), range.c_str());

    if (!IsSetup()) {
        LMRTSP_LOGE("Cannot play media: session not setup");
        return false;
    }

    // Check if this is multi-track or single-track
    bool is_multi_track = false;
    {
        std::lock_guard<std::mutex> lock(tracksMutex_);
        if (!tracks_.empty()) {
            is_multi_track = true;
            // Multi-track: start all track stream managers
            LMRTSP_LOGD("Starting %zu tracks for multi-track session", tracks_.size());
            for (auto &[track_index, track_info] : tracks_) {
                if (!track_info.stream_manager) {
                    LMRTSP_LOGE("Track %d stream manager not available", track_index);
                    continue;
                }
                if (!track_info.stream_manager->Play()) {
                    LMRTSP_LOGE("Failed to start playing track %d", track_index);
                    return false;
                }
                LMRTSP_LOGD("Track %d started playing", track_index);
            }

            // Set playing state
            SetState(ServerSessionStateEnum::PLAYING);

            LMRTSP_LOGD("All tracks started for multi-track session: %s", sessionId_.c_str());
        }
    } // Release lock before calling callback

    // Notify callback for multi-track (outside lock to avoid deadlock)
    if (is_multi_track) {
        if (auto server = rtspServer_.lock()) {
            if (auto listener = server->GetListener()) {
                listener->OnSessionStartPlay(shared_from_this());
            }
        }
        return true;
    }

    // Single-track (legacy mode)
    std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);
    if (!mediaStreamManager_) {
        LMRTSP_LOGE("Media stream manager not initialized");
        return false;
    }

    if (!mediaStreamManager_->Play()) {
        LMRTSP_LOGE("Failed to start playing media stream");
        return false;
    }

    // Set playing state
    SetState(ServerSessionStateEnum::PLAYING);

    LMRTSP_LOGD("Media playback started for session: %s", sessionId_.c_str());

    // Notify listener that session started playing
    if (auto server = rtspServer_.lock()) {
        if (auto listener = server->GetListener()) {
            listener->OnSessionStartPlay(shared_from_this());
        }
    }

    return true;
}

bool RtspServerSession::PauseMedia(const std::string &uri)
{
    LMRTSP_LOGD("Pausing media for URI: %s", uri.c_str());

    if (!IsPlaying()) {
        LMRTSP_LOGE("Cannot pause media: not currently playing");
        return false;
    }

    // Pause media stream manager
    std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);
    if (!mediaStreamManager_) {
        LMRTSP_LOGE("Media stream manager not initialized");
        return false;
    }

    if (!mediaStreamManager_->Pause()) {
        LMRTSP_LOGE("Failed to pause media stream");
        return false;
    }

    // Set paused state
    SetState(ServerSessionStateEnum::PAUSED);

    LMRTSP_LOGD("Media playback paused for session: %s", sessionId_.c_str());

    // Notify callback that session stopped playing
    if (auto server = rtspServer_.lock()) {
        if (auto listener = server->GetListener()) {
            listener->OnSessionStopPlay(sessionId_);
        }
    }

    return true;
}

bool RtspServerSession::TeardownMedia(const std::string &uri)
{
    LMRTSP_LOGD("Tearing down media for URI: %s", uri.c_str());

    // Teardown media stream manager
    {
        std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);
        if (mediaStreamManager_) {
            mediaStreamManager_->Teardown();
            mediaStreamManager_.reset();
        }
    }

    // Reset state to INIT
    SetState(ServerSessionStateEnum::INIT);

    LMRTSP_LOGD("Media teardown completed for session: %s", sessionId_.c_str());

    // Notify callback that session stopped playing
    if (auto server = rtspServer_.lock()) {
        if (auto listener = server->GetListener()) {
            listener->OnSessionStopPlay(sessionId_);
        }
    }

    return true;
}

void RtspServerSession::SetSdpDescription(const std::string &sdp)
{
    sdpDescription_ = sdp;
}

std::string RtspServerSession::GetSdpDescription() const
{
    return sdpDescription_;
}

void RtspServerSession::SetTransportInfo(const std::string &transport)
{
    transportInfo_ = transport;
}

std::string RtspServerSession::GetTransportInfo() const
{
    return transportInfo_;
}

std::shared_ptr<MediaStream> RtspServerSession::GetMediaStream(int track_index)
{
    if (track_index >= 0 && track_index < static_cast<int>(mediaStreams_.size())) {
        return mediaStreams_[track_index];
    }
    return nullptr;
}

const std::vector<std::shared_ptr<MediaStream>> &RtspServerSession::GetMediaStreams() const
{
    return mediaStreams_;
}

bool RtspServerSession::IsPlaying() const
{
    return state_ == ServerSessionStateEnum::PLAYING;
}

bool RtspServerSession::IsPaused() const
{
    return state_ == ServerSessionStateEnum::PAUSED;
}

bool RtspServerSession::IsSetup() const
{
    return state_ == ServerSessionStateEnum::READY || state_ == ServerSessionStateEnum::PLAYING ||
           state_ == ServerSessionStateEnum::PAUSED || state_ == ServerSessionStateEnum::RECORDING;
}

void RtspServerSession::SetState(ServerSessionStateEnum new_state)
{
    auto old_state = state_.load();
    state_ = new_state;
    LMRTSP_LOGD("Session %s state changed: %s -> %s", sessionId_.c_str(), GetStateString(old_state).c_str(),
                GetStateString(new_state).c_str());
}

ServerSessionStateEnum RtspServerSession::GetState() const
{
    return state_;
}

std::string RtspServerSession::GetStateString() const
{
    return GetStateString(state_);
}

std::string RtspServerSession::GetStateString(ServerSessionStateEnum state)
{
    switch (state) {
        case ServerSessionStateEnum::INIT:
            return "INIT";
        case ServerSessionStateEnum::READY:
            return "READY";
        case ServerSessionStateEnum::PLAYING:
            return "PLAYING";
        case ServerSessionStateEnum::PAUSED:
            return "PAUSED";
        case ServerSessionStateEnum::RECORDING:
            return "RECORDING";
        default:
            return "UNKNOWN";
    }
}

void RtspServerSession::UpdateLastActiveTime()
{
    lastActiveTime_ = lmcore::TimeUtils::GetCurrentTimeMs();
}

bool RtspServerSession::IsExpired(uint32_t timeout_seconds) const
{
    int64_t current_time = lmcore::TimeUtils::GetCurrentTimeMs();
    return (current_time - lastActiveTime_) > static_cast<int64_t>(timeout_seconds) * 1000;
}

int64_t RtspServerSession::GetLastActiveTime() const
{
    return lastActiveTime_;
}

std::string RtspServerSession::GenerateSessionId()
{
    return lmcore::UUID::GenerateShort();
}

// RtpTransportParams RtspServerSession::ParseTransportHeader(const std::string &transport) const
// {
//     RtpTransportParams params;
//     // TODO: Implement transport header parsing
//     return params;
// }

void RtspServerSession::SetMediaStreamInfo(std::shared_ptr<MediaStreamInfo> stream_info)
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    mediaStreamInfo_ = stream_info;
    if (stream_info) {
        LMRTSP_LOGI("SetMediaStreamInfo called - codec: %s, stream_path: %s", stream_info->codec.c_str(),
                    stream_info->stream_path.c_str());
    } else {
        LMRTSP_LOGW("SetMediaStreamInfo called with nullptr");
    }
}

std::shared_ptr<MediaStreamInfo> RtspServerSession::GetMediaStreamInfo() const
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    return mediaStreamInfo_;
}

// void RtspServerSession::SetRTPSender(std::shared_ptr<IRTPSender> rtp_sender)
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     rtpSender_ = rtp_sender;
// }
//
// std::shared_ptr<IRTPSender> RtspServerSession::GetRTPSender() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     return rtpSender_;
// }
//
// bool RtspServerSession::HasRTPSender() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     return rtpSender_ != nullptr;
// }

// void RtspServerSession::SetRTPTransportParams(const RtpTransportParams &params)
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     rtpTransportParams_ = params;
// }
//
// RtpTransportParams RtspServerSession::GetRTPTransportParams() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     return rtpTransportParams_;
// }
//
// bool RtspServerSession::HasValidTransport() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     // TODO: Implement transport validation logic
//     return true;
// }

// RtpStatistics RtspServerSession::GetRTPStatistics() const
// {
//     RtpStatistics stats;
//     // TODO: Implement RTP statistics collection
//     return stats;
// }

bool RtspServerSession::PushFrame(const lmrtsp::MediaFrame &frame)
{
    std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);
    if (!mediaStreamManager_) {
        LMRTSP_LOGE("Media stream manager not initialized");
        return false;
    }

    if (!IsPlaying()) {
        LMRTSP_LOGW("Cannot push frame: session not in playing state");
        return false;
    }

    return mediaStreamManager_->PushFrame(frame);
}

bool RtspServerSession::PushFrame(const lmrtsp::MediaFrame &frame, int track_index)
{
    std::lock_guard<std::mutex> lock(tracksMutex_);

    auto it = tracks_.find(track_index);
    if (it == tracks_.end()) {
        LMRTSP_LOGE("Track %d not found", track_index);
        return false;
    }

    if (!it->second.stream_manager) {
        LMRTSP_LOGE("Track %d stream manager not initialized", track_index);
        return false;
    }

    if (!IsPlaying()) {
        LMRTSP_LOGW("Cannot push frame: session not in playing state");
        return false;
    }

    return it->second.stream_manager->PushFrame(frame);
}

std::string RtspServerSession::GetRtpInfo() const
{
    // Check for multi-track
    {
        std::lock_guard<std::mutex> lock(tracksMutex_);
        if (!tracks_.empty()) {
            // Multi-track: generate RTP-Info for all tracks
            std::string rtp_info;
            for (const auto &[track_index, track_info] : tracks_) {
                if (track_info.stream_manager) {
                    std::string track_rtp_info = track_info.stream_manager->GetRtpInfo();
                    if (!track_rtp_info.empty()) {
                        if (!rtp_info.empty()) {
                            rtp_info += ",";
                        }
                        rtp_info += track_rtp_info;
                    }
                }
            }
            return rtp_info;
        }
    }

    // Single-track (legacy)
    std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);
    if (!mediaStreamManager_) {
        return "";
    }

    return mediaStreamManager_->GetRtpInfo();
}

std::string RtspServerSession::GetStreamUri() const
{
    return streamUri_;
}

std::vector<RtspServerSession::TrackInfo> RtspServerSession::GetTracks() const
{
    std::lock_guard<std::mutex> lock(tracksMutex_);
    std::vector<TrackInfo> result;
    for (const auto &[track_index, internal_track] : tracks_) {
        TrackInfo track_info;
        track_info.uri = internal_track.uri;
        track_info.stream_info = internal_track.stream_info;
        track_info.track_index = internal_track.track_index;
        result.push_back(track_info);
    }
    return result;
}

bool RtspServerSession::IsMultiTrack() const
{
    std::lock_guard<std::mutex> lock(tracksMutex_);
    return !tracks_.empty();
}

bool RtspServerSession::SendInterleavedData(uint8_t channel, const uint8_t *data, size_t size)
{
    if (!lmnetSession_) {
        LMRTSP_LOGE("Network session not available");
        return false;
    }

    // Create interleaved frame: $ + channel + length + data
    std::vector<uint8_t> interleavedFrame;
    interleavedFrame.reserve(4 + size);

    interleavedFrame.push_back('$');                // Magic byte
    interleavedFrame.push_back(channel);            // Channel
    interleavedFrame.push_back((size >> 8) & 0xFF); // Length high byte
    interleavedFrame.push_back(size & 0xFF);        // Length low byte

    // Append data
    interleavedFrame.insert(interleavedFrame.end(), data, data + size);

    // Send via network session
    bool ok = lmnetSession_->Send(interleavedFrame.data(), interleavedFrame.size());
    if (!ok) {
        LMRTSP_LOGE("SendInterleavedData failed: channel=%d, payload_size=%zu, frame_size=%zu",
                    static_cast<int>(channel), size, interleavedFrame.size());
    } else {
        LMRTSP_LOGD("SendInterleavedData ok: channel=%d, payload_size=%zu, frame_size=%zu", static_cast<int>(channel),
                    size, interleavedFrame.size());
    }
    return ok;
}

} // namespace lmshao::lmrtsp