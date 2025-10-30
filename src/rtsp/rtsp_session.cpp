/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_session.h"

#include <ctime>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>

#include "../rtp/i_rtp_transport_adapter.h"
#include "internal_logger.h"
#include "lmrtsp/rtsp_headers.h"
#include "lmrtsp/rtsp_media_stream_manager.h"
#include "lmrtsp/rtsp_server.h"
#include "rtsp_response.h"
#include "rtsp_session_state.h"

namespace lmshao::lmrtsp {

RTSPSession::RTSPSession(std::shared_ptr<lmnet::Session> lmnetSession)
    : lmnetSession_(lmnetSession), timeout_(60),
      mediaStreamManager_(std::make_unique<lmshao::lmrtsp::RtspMediaStreamManager>(std::weak_ptr<RTSPSession>()))
{ // Default 60 seconds timeout

    // Generate session ID
    sessionId_ = GenerateSessionId();

    // Initialize last active time
    lastActiveTime_ = std::time(nullptr);

    // Initialize state machine to Initial state
    currentState_ = InitialState::GetInstance();

    LMRTSP_LOGD("RTSPSession created with ID: %s", sessionId_.c_str());
}

RTSPSession::RTSPSession(std::shared_ptr<lmnet::Session> lmnetSession, std::weak_ptr<RTSPServer> server)
    : lmnetSession_(lmnetSession), rtspServer_(server), timeout_(60),
      mediaStreamManager_(std::make_unique<lmshao::lmrtsp::RtspMediaStreamManager>(std::weak_ptr<RTSPSession>()))
{
    // Generate session ID
    sessionId_ = GenerateSessionId();

    // Initialize last active time
    lastActiveTime_ = std::time(nullptr);

    // Initialize state machine to Initial state
    currentState_ = InitialState::GetInstance();

    LMRTSP_LOGD("RTSPSession created with ID: %s and server reference", sessionId_.c_str());
}

RTSPSession::~RTSPSession()
{
    LMRTSP_LOGD("RTSPSession destroyed: %s", sessionId_.c_str());

    // Clean up media streams
    mediaStreams_.clear();
}

RTSPResponse RTSPSession::ProcessRequest(const RTSPRequest &request)
{
    // Update last active time
    UpdateLastActiveTime();

    // Use state machine to process request
    if (!currentState_) {
        // Fallback: initialize to Initial state if not set
        currentState_ = InitialState::GetInstance();
    }

    const std::string &method = request.method_;

    // Delegate to state machine based on method
    if (method == "OPTIONS") {
        return currentState_->OnOptions(this, request);
    } else if (method == "DESCRIBE") {
        return currentState_->OnDescribe(this, request);
    } else if (method == "ANNOUNCE") {
        return currentState_->OnAnnounce(this, request);
    } else if (method == "RECORD") {
        return currentState_->OnRecord(this, request);
    } else if (method == "SETUP") {
        return currentState_->OnSetup(this, request);
    } else if (method == "PLAY") {
        return currentState_->OnPlay(this, request);
    } else if (method == "PAUSE") {
        return currentState_->OnPause(this, request);
    } else if (method == "TEARDOWN") {
        return currentState_->OnTeardown(this, request);
    } else if (method == "GET_PARAMETER") {
        return currentState_->OnGetParameter(this, request);
    } else if (method == "SET_PARAMETER") {
        return currentState_->OnSetParameter(this, request);
    } else {
        // Unknown method
        int cseq = 0;
        auto cseq_it = request.general_header_.find("CSeq");
        if (cseq_it != request.general_header_.end()) {
            cseq = std::stoi(cseq_it->second);
        }
        return RTSPResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    }
}

void RTSPSession::ChangeState(std::shared_ptr<RTSPSessionState> newState)
{
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

std::string RTSPSession::GetClientIP() const
{
    if (lmnetSession_) {
        return lmnetSession_->host;
    }
    return "";
}

uint16_t RTSPSession::GetClientPort() const
{
    if (lmnetSession_) {
        return lmnetSession_->port;
    }
    return 0;
}

std::shared_ptr<lmnet::Session> RTSPSession::GetNetworkSession() const
{
    return lmnetSession_;
}

std::weak_ptr<RTSPServer> RTSPSession::GetRTSPServer() const
{
    return rtspServer_;
}

bool RTSPSession::SetupMedia(const std::string &uri, const std::string &transport)
{
    LMRTSP_LOGD("Setting up media for URI: %s, Transport: %s", uri.c_str(), transport.c_str());

    // Parse transport parameters (removed by request)
    // rtpTransportParams_ = ParseTransportHeader(transport);

    // Create new media stream manager
    std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);

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
        }

        // Server ports will be allocated dynamically (set to 0)
        transportConfig.server_rtp_port = 0;
        transportConfig.server_rtcp_port = 0;
    }

    // Create media stream manager
    mediaStreamManager_ =
        std::make_unique<lmshao::lmrtsp::RtspMediaStreamManager>(std::weak_ptr<RTSPSession>(shared_from_this()));

    // Setup the media stream manager with transport config
    if (!mediaStreamManager_->Setup(transportConfig)) {
        LMRTSP_LOGE("Failed to setup media stream manager");
        mediaStreamManager_.reset();
        return false;
    }

    // Get transport info from media stream manager
    transportInfo_ = mediaStreamManager_->GetTransportInfo();

    // Save stream URI for RTP-Info in PLAY response
    streamUri_ = uri;

    // Set setup flag
    isSetup_ = true;

    LMRTSP_LOGD("Media setup completed for session: %s, Transport: %s", sessionId_.c_str(), transportInfo_.c_str());
    return true;
}

bool RTSPSession::PlayMedia(const std::string &uri, const std::string &range)
{
    LMRTSP_LOGD("Playing media for URI: %s, Range: %s", uri.c_str(), range.c_str());

    if (!isSetup_) {
        LMRTSP_LOGE("Cannot play media: session not setup");
        return false;
    }

    // Start playing media stream manager
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
    isPlaying_ = true;
    isPaused_ = false;

    LMRTSP_LOGD("Media playback started for session: %s", sessionId_.c_str());
    return true;
}

bool RTSPSession::PauseMedia(const std::string &uri)
{
    LMRTSP_LOGD("Pausing media for URI: %s", uri.c_str());

    if (!isPlaying_) {
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
    isPaused_ = true;
    isPlaying_ = false;

    LMRTSP_LOGD("Media playback paused for session: %s", sessionId_.c_str());
    return true;
}

bool RTSPSession::TeardownMedia(const std::string &uri)
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

    // Reset all states
    isPlaying_ = false;
    isPaused_ = false;
    isSetup_ = false;

    LMRTSP_LOGD("Media teardown completed for session: %s", sessionId_.c_str());
    return true;
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
    if (track_index >= 0 && track_index < static_cast<int>(mediaStreams_.size())) {
        return mediaStreams_[track_index];
    }
    return nullptr;
}

const std::vector<std::shared_ptr<MediaStream>> &RTSPSession::GetMediaStreams() const
{
    return mediaStreams_;
}

bool RTSPSession::IsPlaying() const
{
    return isPlaying_;
}

bool RTSPSession::IsPaused() const
{
    return isPaused_;
}

bool RTSPSession::IsSetup() const
{
    return isSetup_;
}

void RTSPSession::UpdateLastActiveTime()
{
    lastActiveTime_ = std::time(nullptr);
}

bool RTSPSession::IsExpired(uint32_t timeout_seconds) const
{
    time_t current_time = std::time(nullptr);
    return (current_time - lastActiveTime_) > timeout_seconds;
}

time_t RTSPSession::GetLastActiveTime() const
{
    return lastActiveTime_;
}

std::string RTSPSession::GenerateSessionId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    return std::to_string(dis(gen));
}

// RTPTransportParams RTSPSession::ParseTransportHeader(const std::string &transport) const
// {
//     RTPTransportParams params;
//     // TODO: Implement transport header parsing
//     return params;
// }

void RTSPSession::SetMediaStreamInfo(std::shared_ptr<MediaStreamInfo> stream_info)
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    mediaStreamInfo_ = stream_info;
}

std::shared_ptr<MediaStreamInfo> RTSPSession::GetMediaStreamInfo() const
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    return mediaStreamInfo_;
}

// void RTSPSession::SetRTPSender(std::shared_ptr<IRTPSender> rtp_sender)
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     rtpSender_ = rtp_sender;
// }
//
// std::shared_ptr<IRTPSender> RTSPSession::GetRTPSender() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     return rtpSender_;
// }
//
// bool RTSPSession::HasRTPSender() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     return rtpSender_ != nullptr;
// }

// void RTSPSession::SetRTPTransportParams(const RTPTransportParams &params)
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     rtpTransportParams_ = params;
// }
//
// RTPTransportParams RTSPSession::GetRTPTransportParams() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     return rtpTransportParams_;
// }
//
// bool RTSPSession::HasValidTransport() const
// {
//     std::lock_guard<std::mutex> lock(mediaInfoMutex_);
//     // TODO: Implement transport validation logic
//     return true;
// }

// RTPStatistics RTSPSession::GetRTPStatistics() const
// {
//     RTPStatistics stats;
//     // TODO: Implement RTP statistics collection
//     return stats;
// }

bool RTSPSession::PushFrame(const lmrtsp::MediaFrame &frame)
{
    std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);
    if (!mediaStreamManager_) {
        LMRTSP_LOGE("Media stream manager not initialized");
        return false;
    }

    if (!isPlaying_) {
        LMRTSP_LOGW("Cannot push frame: session not in playing state");
        return false;
    }

    return mediaStreamManager_->PushFrame(frame);
}

std::string RTSPSession::GetRtpInfo() const
{
    std::lock_guard<std::mutex> lock(mediaStreamManagerMutex_);
    if (!mediaStreamManager_) {
        return "";
    }

    return mediaStreamManager_->GetRtpInfo();
}

std::string RTSPSession::GetStreamUri() const
{
    return streamUri_;
}

bool RTSPSession::SendInterleavedData(uint8_t channel, const uint8_t *data, size_t size)
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
    return lmnetSession_->Send(interleavedFrame.data(), interleavedFrame.size());
}

} // namespace lmshao::lmrtsp