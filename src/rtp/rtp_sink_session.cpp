/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtp_sink_session.h"

#include <lmcore/data_buffer.h>

#include <random>

#include "i_rtp_depacketizer.h"
#include "i_rtp_transport_adapter.h"
#include "internal_logger.h"
#include "lmcore/time_utils.h"
#include "lmrtsp/rtcp_context.h"
#include "lmrtsp/rtp_packet.h"
#include "rtp_depacketizer_h264.h"
#include "udp_rtp_transport_adapter.h"

namespace lmshao::lmrtsp {

namespace {
// Generate random SSRC
uint32_t GenerateRandomSSRC()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(1, 0xFFFFFFFF);
    return dis(gen);
}
} // namespace

// Internal listener class to handle transport data reception
class RtpSinkSession::TransportListener : public UdpRtpTransportAdapterListener {
public:
    explicit TransportListener(RtpSinkSession *session) : session_(session) {}

    void OnRtpDataReceived(std::shared_ptr<lmcore::DataBuffer> buffer) override
    {
        if (!session_ || !buffer) {
            return;
        }
        session_->HandleRtpData(buffer);
    }

    void OnRtcpDataReceived(std::shared_ptr<lmcore::DataBuffer> buffer) override
    {
        if (!session_ || !buffer) {
            return;
        }
        session_->HandleRtcpData(buffer);
    }

private:
    RtpSinkSession *session_;
};

// Internal listener class to bridge depacketizer frames to external listener
class RtpSinkSession::DepacketizerListener : public IRtpDepacketizerListener {
public:
    explicit DepacketizerListener(RtpSinkSession *session) : session_(session) {}

    void OnFrame(const std::shared_ptr<MediaFrame> &frame) override
    {
        if (!session_) {
            return;
        }
        session_->HandleFrame(frame);
    }

    void OnError(int code, const std::string &message) override
    {
        if (!session_) {
            return;
        }
        session_->HandleDepacketizerError(code, message);
    }

private:
    RtpSinkSession *session_;
};

RtpSinkSession::RtpSinkSession() {}

RtpSinkSession::~RtpSinkSession()
{
    Stop();
}

bool RtpSinkSession::Initialize(const RtpSinkSessionConfig &config)
{
    if (initialized_) {
        LMRTSP_LOGE("RtpSinkSession already initialized");
        return false;
    }

    config_ = config;

    // Validate configuration
    if (config_.session_id.empty()) {
        LMRTSP_LOGE("Session ID cannot be empty");
        return false;
    }

    // Ensure transport is configured for SINK mode
    if (config_.transport.mode != TransportConfig::Mode::SINK) {
        LMRTSP_LOGE("Transport must be configured for SINK mode");
        return false;
    }

    // Create transport adapter based on config
    if (config_.transport.type == TransportConfig::Type::UDP) {
        auto udp_adapter = std::make_unique<UdpRtpTransportAdapter>();
        transportListener_ = std::make_shared<TransportListener>(this);
        udp_adapter->SetOnDataListener(transportListener_);
        transportAdapter_ = std::move(udp_adapter);
    } else if (config_.transport.type == TransportConfig::Type::TCP_INTERLEAVED) {
        LMRTSP_LOGE("TCP_INTERLEAVED transport type is not supported in RtpSinkSession");
        return false;
    } else {
        LMRTSP_LOGE("Unsupported transport type: %d", static_cast<int>(config_.transport.type));
        return false;
    }

    // Create video depacketizer based on media type
    if (config_.video_type == MediaType::H264) {
        videoDepacketizer_ = std::make_unique<RtpDepacketizerH264>();
        depacketizerListener_ = std::make_shared<DepacketizerListener>(this);
        videoDepacketizer_->SetListener(depacketizerListener_);
    } else {
        LMRTSP_LOGE("Unsupported video type: %d", static_cast<int>(config_.video_type));
        transportAdapter_.reset();
        return false;
    }

    // Initialize RTCP if enabled
    if (config_.enable_rtcp) {
        rtcpSsrc_ = GenerateRandomSSRC();
        rtcpContext_ = RtcpReceiverContext::Create();
        if (rtcpContext_) {
            // RTCP context will be fully initialized when first RTP packet arrives (to get sender SSRC)
            LMRTSP_LOGI("RTCP receiver context created: SSRC=0x%08x (pending sender SSRC)", rtcpSsrc_);
        } else {
            LMRTSP_LOGW("Failed to create RTCP receiver context");
        }
    }

    initialized_ = true;
    LMRTSP_LOGI("RtpSinkSession initialized successfully for session: %s", config_.session_id.c_str());
    return true;
}

bool RtpSinkSession::Start()
{
    if (!initialized_) {
        LMRTSP_LOGE("RtpSinkSession not initialized");
        return false;
    }

    if (running_) {
        LMRTSP_LOGI("RtpSinkSession already running");
        return true;
    }

    // Setup and start transport
    if (!transportAdapter_->Setup(config_.transport)) {
        LMRTSP_LOGE("Failed to setup transport adapter");
        return false;
    }

    running_ = true;
    LMRTSP_LOGI("RtpSinkSession started successfully for session: %s", config_.session_id.c_str());
    return true;
}

void RtpSinkSession::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;

    // Stop RTCP timer
    if (config_.enable_rtcp) {
        StopRtcpTimer();
    }

    // Close transport
    if (transportAdapter_) {
        transportAdapter_->Close();
    }

    LMRTSP_LOGI("RtpSinkSession stopped for session: %s", config_.session_id.c_str());
}

void RtpSinkSession::SetListener(std::shared_ptr<RtpSinkSessionListener> listener)
{
    listener_ = listener;
    LMRTSP_LOGI("Listener set for session: %s", config_.session_id.c_str());
}

void RtpSinkSession::HandleRtpData(std::shared_ptr<lmcore::DataBuffer> buffer)
{
    if (!running_ || !buffer || !videoDepacketizer_) {
        return;
    }

    // Parse RTP packet from buffer
    auto rtp_packet = RtpPacket::Deserialize(*buffer);
    if (!rtp_packet) {
        LMRTSP_LOGE("Failed to parse RTP packet");
        return;
    }

    // Validate SSRC if configured
    if (config_.expected_ssrc != 0 && rtp_packet->ssrc != config_.expected_ssrc) {
        LMRTSP_LOGW("Received RTP packet with unexpected SSRC: %u (expected: %u)", rtp_packet->ssrc,
                    config_.expected_ssrc);
        return;
    }

    // Validate payload type
    if (rtp_packet->payload_type != config_.video_payload_type) {
        LMRTSP_LOGW("Received RTP packet with unexpected payload type: %u (expected: %u)", rtp_packet->payload_type,
                    config_.video_payload_type);
        return;
    }

    // Initialize RTCP context with sender SSRC on first RTP packet
    if (config_.enable_rtcp && rtcpContext_ && !rtcpContext_->GetExpectedPackets()) {
        rtcpContext_->Initialize(rtcpSsrc_, rtp_packet->ssrc);
        StartRtcpTimer();
        LMRTSP_LOGI("RTCP initialized: receiver SSRC=0x%08x, sender SSRC=0x%08x", rtcpSsrc_, rtp_packet->ssrc);
    }

    // Update RTCP statistics
    if (rtcpContext_) {
        rtcpContext_->OnRtp(rtp_packet->sequence_number, rtp_packet->timestamp, lmcore::TimeUtils::GetCurrentTimeMs(),
                            90000, // 90kHz for video
                            buffer->Size());
    }

    // Check for sequence number gaps (simple detection)
    if (lastSequenceNumber_ != 0) {
        uint16_t expected_seq = lastSequenceNumber_ + 1;
        if (rtp_packet->sequence_number != expected_seq) {
            LMRTSP_LOGW("Sequence number gap detected: got %u, expected %u", rtp_packet->sequence_number, expected_seq);
        }
    }
    lastSequenceNumber_ = rtp_packet->sequence_number;

    // Update timestamp tracking
    lastTimestamp_ = rtp_packet->timestamp;

    // Submit packet to depacketizer
    videoDepacketizer_->SubmitPacket(rtp_packet);

    LMRTSP_LOGD("Processed RTP packet: SSRC=%u, seq=%u, ts=%u, pt=%u, size=%u", rtp_packet->ssrc,
                rtp_packet->sequence_number, rtp_packet->timestamp, rtp_packet->payload_type,
                rtp_packet->payload ? rtp_packet->payload->Size() : 0);
}

void RtpSinkSession::HandleRtcpData(std::shared_ptr<lmcore::DataBuffer> buffer)
{
    if (!rtcpContext_ || !buffer) {
        return;
    }

    // Process RTCP packet (SR, BYE, etc.)
    rtcpContext_->OnRtcp(buffer->Data(), buffer->Size());
    LMRTSP_LOGD("Processed RTCP packet: size=%zu", buffer->Size());
}

void RtpSinkSession::HandleFrame(const std::shared_ptr<MediaFrame> &frame)
{
    if (!running_ || !frame) {
        return;
    }

    // Forward frame to listener (check if still alive)
    if (auto listener = listener_.lock()) {
        listener->OnFrame(frame);
    }

    LMRTSP_LOGD("Forwarded decoded frame: type=%d, size=%u, timestamp=%u", static_cast<int>(frame->media_type),
                frame->data ? frame->data->Size() : 0, frame->timestamp);
}

void RtpSinkSession::HandleDepacketizerError(int code, const std::string &message)
{
    LMRTSP_LOGE("Depacketizer error: %d - %s", code, message.c_str());

    // Forward error to listener (check if still alive)
    if (auto listener = listener_.lock()) {
        listener->OnError(code, message);
    }
}

void RtpSinkSession::StartRtcpTimer()
{
    if (!rtcpTimer_) {
        rtcpTimer_ = std::make_unique<lmcore::AsyncTimer>(1);
        rtcpTimer_->Start();
    }

    // Schedule repeating RTCP report
    rtcpTimerId_ = rtcpTimer_->ScheduleRepeating([this]() { SendRtcpReport(); }, config_.rtcp_interval_ms);

    LMRTSP_LOGI("RTCP timer started: interval=%ums", config_.rtcp_interval_ms);
}

void RtpSinkSession::StopRtcpTimer()
{
    if (rtcpTimer_ && rtcpTimerId_ != 0) {
        rtcpTimer_->Cancel(rtcpTimerId_);
        rtcpTimer_->Stop();
        rtcpTimerId_ = 0;
        LMRTSP_LOGI("RTCP timer stopped");
    }
}

void RtpSinkSession::SendRtcpReport()
{
    if (!rtcpContext_ || !transportAdapter_) {
        return;
    }

    // Create compound packet (RR + SDES) if CNAME is provided, otherwise just RR
    std::shared_ptr<lmcore::DataBuffer> rtcpPacket;

    if (!config_.rtcp_cname.empty()) {
        rtcpPacket = rtcpContext_->CreateCompoundPacket(config_.rtcp_cname, config_.rtcp_name);
    } else {
        rtcpPacket = rtcpContext_->CreateRtcpRr();
    }

    if (rtcpPacket && rtcpPacket->Size() > 0) {
        bool success = transportAdapter_->SendRtcpPacket(rtcpPacket->Data(), rtcpPacket->Size());
        if (success) {
            LMRTSP_LOGD("RTCP report sent: size=%zu, lost=%zu, jitter=%u", rtcpPacket->Size(), rtcpContext_->GetLost(),
                        rtcpContext_->GetJitter());
        } else {
            LMRTSP_LOGW("Failed to send RTCP report");
        }
    }
}

} // namespace lmshao::lmrtsp