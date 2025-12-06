/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_client_session.h"

#include <lmcore/url.h>
#include <lmcore/uuid.h>

#include <iostream>
#include <random>
#include <regex>
#include <sstream>

#include "internal_logger.h"
#include "lmrtsp/media_stream_info.h"
#include "lmrtsp/rtsp_client.h"

namespace lmshao::lmrtsp {

RtspClientSession::RtspClientSession(const std::string &url, std::weak_ptr<RtspClient> client)
    : url_(url), client_(client)
{

    // Generate session ID
    sessionId_ = lmcore::UUID::GenerateShort();

    // Initialize transport config for UDP
    transportConfig_.type = TransportConfig::Type::UDP;
    transportConfig_.mode = TransportConfig::Mode::SINK;

    // Allocate client ports
    AllocateClientPorts();
}

RtspClientSession::~RtspClientSession()
{
    Cleanup();
}

bool RtspClientSession::Initialize()
{
    try {
        LMRTSP_LOGI("Initializing RTSP client session: %s for URL: %s", sessionId_.c_str(), url_.c_str());

        // Extract media path from URL
        auto parsed_url = lmcore::URL::Parse(url_);
        if (parsed_url && parsed_url->IsRTSP()) {
            mediaPath_ = parsed_url->Path();
            if (mediaPath_.empty()) {
                mediaPath_ = "/";
            }
        } else {
            mediaPath_ = "/";
        }

        SetState(RtspClientSessionState::INIT);
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception initializing session: %s", e.what());
        return false;
    }
}

void RtspClientSession::Cleanup()
{
    try {
        LMRTSP_LOGI("Cleaning up RTSP client session: %s", sessionId_.c_str());

        StopRtpSession();
        SetState(RtspClientSessionState::TEARDOWN);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception during cleanup: %s", e.what());
    }
}

bool RtspClientSession::HandleDescribeResponse(const std::string &sdp)
{
    try {
        LMRTSP_LOGD("Handling DESCRIBE response for session: %s", sessionId_.c_str());

        sdpDescription_ = sdp;

        if (!ParseSDP(sdp)) {
            LMRTSP_LOGE("Failed to parse SDP");
            return false;
        }

        SetState(RtspClientSessionState::READY);

        // Notify callback
        if (auto client = client_.lock()) {
            auto listener = client->GetListener();
            if (listener) {
                listener->OnDescribeReceived(url_, sdp);
            }
        }

        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception handling DESCRIBE response: %s", e.what());
        return false;
    }
}

bool RtspClientSession::HandleSetupResponse(const std::string &session_id, const std::string &transport)
{
    try {
        LMRTSP_LOGD("Handling SETUP response for session: %s", sessionId_.c_str());

        if (!session_id.empty()) {
            sessionId_ = session_id;
        }

        transportInfo_ = transport;

        // Parse transport info to extract server ports
        std::regex server_port_regex(R"(server_port=(\d+)-(\d+))");
        std::smatch matches;
        if (std::regex_search(transport, matches, server_port_regex)) {
            uint16_t server_rtp_port = static_cast<uint16_t>(std::stoi(matches[1].str()));
            uint16_t server_rtcp_port = static_cast<uint16_t>(std::stoi(matches[2].str()));

            transportConfig_.server_rtp_port = server_rtp_port;
            transportConfig_.server_rtcp_port = server_rtcp_port;

            LMRTSP_LOGI("Parsed server ports: RTP=%u, RTCP=%u", server_rtp_port, server_rtcp_port);
        }

        // Setup RTP session
        if (!SetupRtpSession()) {
            LMRTSP_LOGE("Failed to setup RTP session");
            return false;
        }

        SetState(RtspClientSessionState::READY);

        // Notify callback
        if (auto client = client_.lock()) {
            auto listener = client->GetListener();
            if (listener) {
                listener->OnSetupReceived(url_, sessionId_, transport);
            }
        }

        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception handling SETUP response: %s", e.what());
        return false;
    }
}

bool RtspClientSession::HandlePlayResponse(const std::string &rtp_info)
{
    try {
        LMRTSP_LOGD("Handling PLAY response for session: %s", sessionId_.c_str());

        if (!StartRtpSession()) {
            LMRTSP_LOGE("Failed to start RTP session");
            return false;
        }

        SetState(RtspClientSessionState::PLAYING);

        // Notify callback
        if (auto client = client_.lock()) {
            auto listener = client->GetListener();
            if (listener) {
                listener->OnPlayReceived(url_, sessionId_, rtp_info);
            }
        }

        LMRTSP_LOGI("Session %s is now playing", sessionId_.c_str());
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception handling PLAY response: %s", e.what());
        return false;
    }
}

bool RtspClientSession::HandlePauseResponse()
{
    try {
        LMRTSP_LOGD("Handling PAUSE response for session: %s", sessionId_.c_str());

        StopRtpSession();
        SetState(RtspClientSessionState::PAUSED);

        // Notify callback
        if (auto client = client_.lock()) {
            auto listener = client->GetListener();
            if (listener) {
                listener->OnPauseReceived(url_, sessionId_);
            }
        }

        LMRTSP_LOGI("Session %s is now paused", sessionId_.c_str());
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception handling PAUSE response: %s", e.what());
        return false;
    }
}

bool RtspClientSession::HandleTeardownResponse()
{
    try {
        LMRTSP_LOGD("Handling TEARDOWN response for session: %s", sessionId_.c_str());

        StopRtpSession();
        SetState(RtspClientSessionState::TEARDOWN);

        // Notify callback
        if (auto client = client_.lock()) {
            auto listener = client->GetListener();
            if (listener) {
                listener->OnTeardownReceived(url_, sessionId_);
            }
        }

        LMRTSP_LOGI("Session %s has been torn down", sessionId_.c_str());
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception handling TEARDOWN response: %s", e.what());
        return false;
    }
}

void RtspClientSession::SetState(RtspClientSessionState new_state)
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    RtspClientSessionState old_state = state_.load();
    state_.store(new_state);

    LMRTSP_LOGD("Session %s state changed: %s -> %s", sessionId_.c_str(), GetStateString(old_state).c_str(),
                GetStateString(new_state).c_str());

    // Notify callback
    if (auto client = client_.lock()) {
        auto listener = client->GetListener();
        if (listener) {
            listener->OnStateChanged(url_, GetStateString(old_state), GetStateString(new_state));
        }
    }
}

RtspClientSessionState RtspClientSession::GetState() const
{
    return state_.load();
}

std::string RtspClientSession::GetStateString() const
{
    return GetStateString(state_.load());
}

std::string RtspClientSession::GetStateString(RtspClientSessionState state)
{
    switch (state) {
        case RtspClientSessionState::INIT:
            return "INIT";
        case RtspClientSessionState::READY:
            return "READY";
        case RtspClientSessionState::PLAYING:
            return "PLAYING";
        case RtspClientSessionState::PAUSED:
            return "PAUSED";
        case RtspClientSessionState::TEARDOWN:
            return "TEARDOWN";
        default:
            return "UNKNOWN";
    }
}

std::shared_ptr<MediaStreamInfo> RtspClientSession::GetMediaStreamInfo() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return mediaStreamInfo_;
}

std::string RtspClientSession::GetMediaPath() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return mediaPath_;
}

bool RtspClientSession::StartRtpSession()
{
    try {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        if (rtpSession_ && !rtpSessionStarted_) {
            if (rtpSession_->Start()) {
                rtpSessionStarted_ = true;
                LMRTSP_LOGI("RTP session started for session: %s", sessionId_.c_str());
                return true;
            } else {
                LMRTSP_LOGE("Failed to start RTP session");
                return false;
            }
        }

        return rtpSessionStarted_;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception starting RTP session: %s", e.what());
        return false;
    }
}

void RtspClientSession::StopRtpSession()
{
    try {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        if (rtpSession_ && rtpSessionStarted_) {
            rtpSession_->Stop();
            rtpSessionStarted_ = false;
            LMRTSP_LOGI("RTP session stopped for session: %s", sessionId_.c_str());
        }
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception stopping RTP session: %s", e.what());
    }
}

void RtspClientSession::SetTransportConfig(const TransportConfig &config)
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    transportConfig_ = config;
}

TransportConfig RtspClientSession::GetTransportConfig() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return transportConfig_;
}

void RtspClientSession::OnFrame(const std::shared_ptr<MediaFrame> &frame)
{
    try {
        if (!frame) {
            LMRTSP_LOGW("Received null frame");
            return;
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            framesReceived_++;
            if (frame->data) {
                bytesReceived_ += frame->data->Size();
            }
        }

        LMRTSP_LOGD("Received frame: %zu bytes, timestamp: %u", frame->data ? frame->data->Size() : 0,
                    frame->timestamp);

        // Forward to callback
        if (auto client = client_.lock()) {
            auto listener = client->GetListener();
            if (listener) {
                listener->OnFrame(frame);
            }
        }
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception handling frame: %s", e.what());
    }
}

void RtspClientSession::OnError(int code, const std::string &message)
{
    LMRTSP_LOGE("RTP session error: %d - %s", code, message.c_str());

    // Forward to callback
    if (auto client = client_.lock()) {
        auto listener = client->GetListener();
        if (listener) {
            listener->OnError(url_, code, message);
        }
    }
}

// Private methods
bool RtspClientSession::ParseSDP(const std::string &sdp)
{
    try {
        LMRTSP_LOGD("Parsing SDP:\n%s", sdp.c_str());

        // Create media stream info
        mediaStreamInfo_ = std::make_shared<MediaStreamInfo>();
        mediaStreamInfo_->stream_path = mediaPath_;

        // Parse SDP for H.264 video
        std::istringstream sdp_stream(sdp);
        std::string line;
        bool video_found = false;

        while (std::getline(sdp_stream, line)) {
            // Remove carriage return if present
            line.erase(line.find_last_not_of("\r") + 1);

            if (line.empty())
                continue;

            if (line[0] == 'v') {
                // Version line - not stored in MediaStreamInfo
                LMRTSP_LOGD("SDP Version: %s", line.substr(2).c_str());
            } else if (line[0] == 's') {
                // Session name - not stored in MediaStreamInfo
                LMRTSP_LOGD("SDP Session: %s", line.substr(2).c_str());
            } else if (line[0] == 'm') {
                // Media description
                if (line.find("video") != std::string::npos && line.find("RTP/AVP") != std::string::npos) {
                    video_found = true;
                    // Extract payload type
                    std::istringstream media_line(line);
                    std::string media_type, transport, temp;
                    uint16_t port;
                    media_line >> temp >> port >> media_type >> transport;
                    if (media_line >> temp) {
                        mediaStreamInfo_->payload_type = static_cast<uint8_t>(std::stoi(temp));
                    }
                }
            } else if (line[0] == 'a' && video_found) {
                // Attribute lines
                if (line.find("rtpmap:") != std::string::npos) {
                    // Parse RTP map
                    std::istringstream attr_line(line.substr(2)); // Skip "a="
                    std::string rtpmap_str;
                    attr_line >> rtpmap_str;
                    if (rtpmap_str.find("96") != std::string::npos) {
                        std::string codec_info;
                        attr_line >> codec_info;
                        if (codec_info.find("H264") != std::string::npos) {
                            mediaStreamInfo_->codec = "H264";
                        }
                    }
                } else if (line.find("fmtp:") != std::string::npos) {
                    // Parse format parameters
                    mediaStreamInfo_->profile_level = line.substr(line.find("fmtp:") + 5);
                }
            }
        }

        if (video_found) {
            LMRTSP_LOGI("Successfully parsed SDP for H.264 video stream");
            return true;
        } else {
            LMRTSP_LOGE("No video stream found in SDP");
            return false;
        }
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception parsing SDP: %s", e.what());
        return false;
    }
}

bool RtspClientSession::SetupRtpSession()
{
    try {
        if (rtpSession_) {
            LMRTSP_LOGW("RTP session already exists");
            return true;
        }

        // Configure RTP sink session
        RtpSinkSessionConfig config;
        config.session_id = sessionId_;
        config.expected_ssrc = mediaStreamInfo_->ssrc;
        config.video_type = MediaType::H264;
        config.video_payload_type = mediaStreamInfo_->payload_type;
        config.transport = transportConfig_;

        // Create RTP session
        rtpSession_ = std::make_shared<RtpSinkSession>();
        if (!rtpSession_->Initialize(config)) {
            LMRTSP_LOGE("Failed to initialize RTP sink session");
            return false;
        }

        // Set listener
        rtpSession_->SetListener(std::static_pointer_cast<RtpSinkSessionListener>(shared_from_this()));

        LMRTSP_LOGI("RTP session configured successfully");
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception setting up RTP session: %s", e.what());
        return false;
    }
}

std::string RtspClientSession::AllocateClientPorts()
{
    try {
        // Allocate client ports (simplified - in real implementation should check for port availability)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> port_dis(10000, 20000);

        clientRtpPort_ = static_cast<uint16_t>(port_dis(gen));
        clientRtcpPort_ = clientRtpPort_ + 1;

        transportConfig_.client_rtp_port = clientRtpPort_;
        transportConfig_.client_rtcp_port = clientRtcpPort_;

        LMRTSP_LOGI("Allocated client ports: RTP=%u, RTCP=%u", clientRtpPort_, clientRtcpPort_);

        return GenerateTransportHeader();
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception allocating client ports: %s", e.what());
        return "";
    }
}

std::string RtspClientSession::GenerateTransportHeader()
{
    std::ostringstream transport;
    transport << "RTP/AVP;unicast;client_port=" << clientRtpPort_ << "-" << clientRtcpPort_;
    return transport.str();
}

} // namespace lmshao::lmrtsp