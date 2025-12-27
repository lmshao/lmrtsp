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
#include "rtsp_client_state.h"

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

    // Initialize state machine to Init state
    currentState_ = ClientInitState::GetInstance();
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

        // State is already initialized to INIT in constructor, no need to set again
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
            // Extract only the session ID part (before semicolon)
            // Example: "F42364D7;timeout=65" -> "F42364D7"
            size_t semicolon_pos = session_id.find(';');
            if (semicolon_pos != std::string::npos) {
                sessionId_ = session_id.substr(0, semicolon_pos);
                LMRTSP_LOGD("Parsed Session ID: %s (from: %s)", sessionId_.c_str(), session_id.c_str());
            } else {
                sessionId_ = session_id;
            }
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

        // Start RTP session immediately to avoid missing initial packets
        if (!StartRtpSession()) {
            LMRTSP_LOGE("Failed to start RTP session");
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

        // Parse SDP for video/audio media
        std::istringstream sdp_stream(sdp);
        std::string line;
        bool media_found = false;
        std::string current_media_type;
        uint8_t current_payload_type = 0;

        while (std::getline(sdp_stream, line)) {
            // Remove carriage return if present
            line.erase(line.find_last_not_of("\r") + 1);

            if (line.empty())
                continue;

            if (line[0] == 'v') {
                // Version line
                LMRTSP_LOGD("SDP Version: %s", line.substr(2).c_str());
            } else if (line[0] == 's') {
                // Session name
                LMRTSP_LOGD("SDP Session: %s", line.substr(2).c_str());
            } else if (line[0] == 'm') {
                // Media description: m=<media> <port> <proto> <fmt>
                // Example: m=video 0 RTP/AVP 96
                // Example: m=video 0 RTP/AVP 33  (for MPEG-2 TS)
                // Example: m=audio 0 RTP/AVP 97
                if (line.find("RTP/AVP") != std::string::npos) {
                    // Parse: m=video 0 RTP/AVP 96
                    std::istringstream media_line(line.substr(2)); // Skip "m="
                    std::string media_type, transport;
                    uint16_t port;
                    media_line >> media_type >> port >> transport;

                    // Check if it's video or audio
                    if (media_type == "video" || media_type == "audio") {
                        current_media_type = media_type;

                        // Extract payload type(s)
                        std::string pt_str;
                        if (media_line >> pt_str) {
                            current_payload_type = static_cast<uint8_t>(std::stoi(pt_str));
                            mediaStreamInfo_->payload_type = current_payload_type;
                            media_found = true;
                            LMRTSP_LOGD("Found media: type=%s, payload_type=%u", current_media_type.c_str(),
                                        current_payload_type);
                        }
                    }
                }
            } else if (line[0] == 'a') {
                // Attribute lines (parse regardless of media_found to get control URL)
                if (line.find("control:") != std::string::npos) {
                    // Parse control URL
                    size_t control_pos = line.find("control:");
                    if (control_pos != std::string::npos) {
                        controlUrl_ = line.substr(control_pos + 8); // Skip "control:"
                        // Trim whitespace
                        controlUrl_.erase(0, controlUrl_.find_first_not_of(" \t\r\n"));
                        controlUrl_.erase(controlUrl_.find_last_not_of(" \t\r\n") + 1);
                        LMRTSP_LOGI("Found control URL: %s", controlUrl_.c_str());
                    }
                } else if (media_found && line.find("rtpmap:") != std::string::npos) {
                    // Parse RTP map: a=rtpmap:<payload type> <encoding name>/<clock rate>
                    // Example: a=rtpmap:96 H264/90000
                    // Example: a=rtpmap:97 H265/90000
                    // Example: a=rtpmap:33 MP2T/90000
                    // Example: a=rtpmap:96 mpeg4-generic/44100/2
                    std::istringstream attr_line(line.substr(2)); // Skip "a="
                    std::string rtpmap_str;
                    attr_line >> rtpmap_str;

                    // Extract payload type and encoding
                    size_t colon_pos = rtpmap_str.find(':');
                    if (colon_pos != std::string::npos) {
                        uint8_t pt = static_cast<uint8_t>(std::stoi(rtpmap_str.substr(colon_pos + 1)));
                        std::string encoding;
                        attr_line >> encoding;

                        // Extract encoding name (before '/')
                        size_t slash_pos = encoding.find('/');
                        if (slash_pos != std::string::npos) {
                            encoding = encoding.substr(0, slash_pos);
                        }

                        // Identify codec
                        if (pt == current_payload_type) {
                            if (encoding == "H264") {
                                mediaStreamInfo_->codec = "H264";
                                LMRTSP_LOGI("Detected H.264 codec (PT=%u)", pt);
                            } else if (encoding == "H265") {
                                mediaStreamInfo_->codec = "H265";
                                LMRTSP_LOGI("Detected H.265 codec (PT=%u)", pt);
                            } else if (encoding == "MP2T") {
                                mediaStreamInfo_->codec = "MP2T";
                                LMRTSP_LOGI("Detected MPEG-2 TS codec (PT=%u)", pt);
                            } else if (encoding == "mpeg4-generic" || encoding == "MP4A-LATM") {
                                mediaStreamInfo_->codec = "AAC";
                                LMRTSP_LOGI("Detected AAC codec (PT=%u)", pt);
                            } else {
                                mediaStreamInfo_->codec = encoding;
                                LMRTSP_LOGI("Detected codec: %s (PT=%u)", encoding.c_str(), pt);
                            }
                        }
                    }
                } else if (line.find("fmtp:") != std::string::npos) {
                    // Parse format parameters (for H.264/H.265 SPS/PPS, AAC config, etc.)
                    mediaStreamInfo_->profile_level = line.substr(line.find("fmtp:") + 5);
                    LMRTSP_LOGD("Format parameters: %s", mediaStreamInfo_->profile_level.c_str());
                }
            }
        }

        if (media_found) {
            if (mediaStreamInfo_->codec.empty()) {
                // Try to infer codec from payload type
                if (current_payload_type == 33) {
                    mediaStreamInfo_->codec = "MP2T";
                    LMRTSP_LOGI("Inferred MPEG-2 TS from payload type 33");
                } else {
                    mediaStreamInfo_->codec = "Unknown";
                    LMRTSP_LOGW("Could not determine codec, using Unknown");
                }
            }

            LMRTSP_LOGI("Successfully parsed SDP: codec=%s, payload_type=%u", mediaStreamInfo_->codec.c_str(),
                        mediaStreamInfo_->payload_type);
            return true;
        } else {
            LMRTSP_LOGE("No media stream found in SDP");
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

        // Determine video type from codec
        if (mediaStreamInfo_->codec == "MP2T") {
            config.video_type = MediaType::MP2T;
        } else if (mediaStreamInfo_->codec == "H265" || mediaStreamInfo_->codec == "HEVC") {
            config.video_type = MediaType::H265;
        } else if (mediaStreamInfo_->codec == "AAC") {
            config.video_type = MediaType::AAC;
        } else {
            // Default to H264
            config.video_type = MediaType::H264;
        }

        config.video_payload_type = mediaStreamInfo_->payload_type;
        config.transport = transportConfig_;

        LMRTSP_LOGI("Creating RTP sink session: codec=%s, video_type=%d, payload_type=%u",
                    mediaStreamInfo_->codec.c_str(), static_cast<int>(config.video_type), config.video_payload_type);

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

        // Generate and save transport info for SETUP request
        transportInfo_ = GenerateTransportHeader();
        return transportInfo_;
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

void RtspClientSession::ChangeState(std::shared_ptr<RtspClientStateMachine> new_state)
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    if (currentState_) {
        LMRTSP_LOGD("Session %s state machine: %s -> %s", sessionId_.c_str(), currentState_->GetName().c_str(),
                    new_state ? new_state->GetName().c_str() : "null");
    }
    currentState_ = new_state;
}

std::shared_ptr<RtspClientStateMachine> RtspClientSession::GetCurrentState() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return currentState_;
}

} // namespace lmshao::lmrtsp