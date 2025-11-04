/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtp_source_session.h"

#include <random>

#include "i_rtp_packetizer.h"
#include "i_rtp_transport_adapter.h"
#include "internal_logger.h"
#include "lmcore/time_utils.h"
#include "lmrtsp/rtcp_context.h"
#include "rtp_packetizer_aac.h"
#include "rtp_packetizer_h264.h"
#include "rtp_packetizer_ts.h"
#include "tcp_interleaved_transport_adapter.h"
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

// Generate random sequence number
uint16_t GenerateRandomSequenceNumber()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint16_t> dis(1, 0xFFFF);
    return dis(gen);
}
} // namespace

// Helper class to handle packetized RTP packets
class RtpSourceSession::PacketizerListener : public IRtpPacketizerListener {
public:
    PacketizerListener(IRtpTransportAdapter *transport, RtcpSenderContext *rtcpContext)
        : transport_(transport), rtcpContext_(rtcpContext)
    {
    }

    void SetRtcpContext(RtcpSenderContext *context) { rtcpContext_ = context; }

    void OnPacket(const std::shared_ptr<RtpPacket> &packet) override
    {
        if (!transport_ || !packet) {
            LMRTSP_LOGE("Invalid packet or transport");
            return;
        }

        // Serialize packet and send
        auto serialized = packet->Serialize();
        if (serialized && serialized->Size() > 0) {
            bool success = transport_->SendPacket(serialized->Data(), serialized->Size());
            if (!success) {
                LMRTSP_LOGE("Failed to send RTP packet - SSRC %u, seq %u, size %u", packet->ssrc,
                            packet->sequence_number, serialized->Size());
            } else {
                LMRTSP_LOGD("Sent RTP packet - SSRC %u, seq %u, timestamp %u, payload type %u, size %u", packet->ssrc,
                            packet->sequence_number, packet->timestamp, packet->payload_type, serialized->Size());

                // Update RTCP statistics
                if (rtcpContext_) {
                    rtcpContext_->OnRtp(packet->sequence_number, packet->timestamp,
                                        lmcore::TimeUtils::GetCurrentTimeMs(), 90000, // 90kHz for video
                                        serialized->Size());
                }
            }
        }
    }

    void OnError(int code, const std::string &message) override
    {
        // Simple error handling - in real implementation, this would be logged
        LMRTSP_LOGE("Packetizer error: %d - %s", code, message.c_str());
        (void)code;
        (void)message;
    }

private:
    IRtpTransportAdapter *transport_;
    RtcpSenderContext *rtcpContext_;
};

RtpSourceSession::RtpSourceSession() {}

RtpSourceSession::~RtpSourceSession()
{
    Stop();
}

bool RtpSourceSession::Initialize(const RtpSourceSessionConfig &config)
{
    if (initialized_) {
        return false; // Already initialized
    }

    config_ = config;

    // Generate SSRC if not provided
    if (config_.ssrc == 0) {
        config_.ssrc = GenerateRandomSSRC();
    }

    // Initialize sequence number and timestamp
    sequenceNumber_ = GenerateRandomSequenceNumber();
    timestamp_ = 0;

    // Create transport adapter based on config
    if (config_.transport.type == TransportConfig::Type::UDP) {
        transportAdapter_ = std::make_unique<UdpRtpTransportAdapter>();
    } else if (config_.transport.type == TransportConfig::Type::TCP_INTERLEAVED) {
        // For TCP interleaved, we need RTSP session
        if (config_.rtsp_session.expired()) {
            LMRTSP_LOGE("TCP_INTERLEAVED transport requires valid RTSP session");
            return false;
        }
        transportAdapter_ = std::make_unique<TcpInterleavedTransportAdapter>(config_.rtsp_session);
        LMRTSP_LOGI("Created TCP interleaved transport adapter");
    } else {
        return false; // Unsupported transport type
    }

    // Setup transport immediately (needed for port allocation)
    if (transportAdapter_) {
        if (!transportAdapter_->Setup(config_.transport)) {
            LMRTSP_LOGE("Failed to setup transport in Initialize");
            transportAdapter_.reset();
            return false;
        }
    }

    // Create video packetizer (note: RTCP context will be initialized later)
    if (config_.video_type == MediaType::H264) {
        videoPacketizer_ =
            std::make_unique<RtpPacketizerH264>(config_.ssrc, sequenceNumber_, config_.video_payload_type,
                                                90000, // H264 clock rate
                                                config_.mtu_size);
        // Set up listener for video packetizer (RTCP context set after initialization)
        videoListener_ = std::static_pointer_cast<IRtpPacketizerListener>(
            std::make_shared<PacketizerListener>(transportAdapter_.get(), nullptr));
        videoPacketizer_->SetListener(videoListener_);
    } else if (config_.video_type == MediaType::MP2T) {
        auto tsPacketizer = std::make_unique<RtpPacketizerTs>();
        tsPacketizer->SetSsrc(config_.ssrc);
        tsPacketizer->SetPayloadType(config_.video_payload_type);
        tsPacketizer->SetMtuSize(config_.mtu_size);
        videoPacketizer_ = std::move(tsPacketizer);
        // Set up listener for TS packetizer
        videoListener_ = std::static_pointer_cast<IRtpPacketizerListener>(
            std::make_shared<PacketizerListener>(transportAdapter_.get(), nullptr));
        videoPacketizer_->SetListener(videoListener_);
    } else if (config_.video_type == MediaType::AAC) {
        videoPacketizer_ = std::make_unique<RtpPacketizerAac>(config_.ssrc, sequenceNumber_, config_.video_payload_type,
                                                              48000, // AAC clock rate (will be updated from stream)
                                                              config_.mtu_size);
        // Set up listener for AAC packetizer
        videoListener_ = std::static_pointer_cast<IRtpPacketizerListener>(
            std::make_shared<PacketizerListener>(transportAdapter_.get(), nullptr));
        videoPacketizer_->SetListener(videoListener_);
    }

    // Check if video packetizer was created
    if (!videoPacketizer_) {
        transportAdapter_.reset();
        return false;
    }

    // Initialize RTCP if enabled
    if (config_.enable_rtcp) {
        rtcpContext_ = RtcpSenderContext::Create();
        if (rtcpContext_) {
            rtcpContext_->Initialize(config_.ssrc, config_.ssrc);

            // Update listener with RTCP context
            if (videoListener_) {
                auto listener = std::static_pointer_cast<PacketizerListener>(videoListener_);
                listener->SetRtcpContext(rtcpContext_.get());
            }

            LMRTSP_LOGI("RTCP sender context initialized: SSRC=0x%08x", config_.ssrc);
        } else {
            LMRTSP_LOGW("Failed to create RTCP sender context");
        }
    }

    initialized_ = true;
    return true;
}

bool RtpSourceSession::Start()
{
    if (!initialized_) {
        LMRTSP_LOGE("Not initialized");
        return false; // Not initialized
    }

    if (running_) {
        LMRTSP_LOGI("Already running");
        return true; // Already running
    }

    // Transport is already setup in Initialize(), just mark as running
    if (!transportAdapter_ || !transportAdapter_->IsActive()) {
        LMRTSP_LOGE("Transport not ready");
        return false;
    }

    running_ = true;

    // Start RTCP timer if enabled
    if (config_.enable_rtcp && rtcpContext_) {
        StartRtcpTimer();
    }

    LMRTSP_LOGD("RTP source session started");
    return true;
}

void RtpSourceSession::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;

    // Stop RTCP timer
    if (config_.enable_rtcp) {
        StopRtcpTimer();
    }

    // Clean up packetizers
    videoPacketizer_.reset();

    // Clean up transport
    if (transportAdapter_) {
        transportAdapter_->Close();
        transportAdapter_.reset();
    }
}

std::string RtpSourceSession::GetTransportInfo() const
{
    if (transportAdapter_) {
        return transportAdapter_->GetTransportInfo();
    }
    return "";
}

bool RtpSourceSession::SendFrame(const std::shared_ptr<MediaFrame> &frame)
{
    LMRTSP_LOGI("SendFrame called - running: %s, frame: %s", running_ ? "true" : "false", frame ? "valid" : "null");

    if (!running_ || !frame) {
        LMRTSP_LOGE("SendFrame failed - running: %s, frame: %s", running_ ? "true" : "false", frame ? "valid" : "null");
        return false;
    }

    // Support H264, MP2T and AAC frames
    if ((frame->media_type != MediaType::H264 && frame->media_type != MediaType::MP2T &&
         frame->media_type != MediaType::AAC) ||
        !videoPacketizer_) {
        LMRTSP_LOGE("SendFrame failed - media_type: %d, video_packetizer: %s", static_cast<int>(frame->media_type),
                    videoPacketizer_ ? "valid" : "null");
        return false; // Unsupported media type or no video packetizer
    }

    LMRTSP_LOGI("About to submit frame to packetizer - frame size: %u", frame->data ? frame->data->Size() : 0);

    // Submit frame for packetization
    // The listener is already set up during initialization
    try {
        videoPacketizer_->SubmitFrame(frame);
        LMRTSP_LOGI("Frame submitted to packetizer successfully");
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in SubmitFrame: %s", e.what());
        return false;
    } catch (...) {
        LMRTSP_LOGE("Unknown exception in SubmitFrame");
        return false;
    }

    LMRTSP_LOGI("SendFrame completed successfully");
    return true;
}

void RtpSourceSession::StartRtcpTimer()
{
    if (!rtcpTimer_) {
        rtcpTimer_ = std::make_unique<lmcore::AsyncTimer>(1);
        rtcpTimer_->Start();
    }

    // Schedule repeating RTCP report
    rtcpTimerId_ = rtcpTimer_->ScheduleRepeating([this]() { SendRtcpReport(); }, config_.rtcp_interval_ms);

    LMRTSP_LOGI("RTCP timer started: interval=%ums", config_.rtcp_interval_ms);
}

void RtpSourceSession::StopRtcpTimer()
{
    if (rtcpTimer_ && rtcpTimerId_ != 0) {
        rtcpTimer_->Cancel(rtcpTimerId_);
        rtcpTimer_->Stop();
        rtcpTimerId_ = 0;
        LMRTSP_LOGI("RTCP timer stopped");
    }
}

void RtpSourceSession::SendRtcpReport()
{
    if (!rtcpContext_ || !transportAdapter_) {
        return;
    }

    // Create compound packet (SR + SDES) if CNAME is provided, otherwise just SR
    std::shared_ptr<lmcore::DataBuffer> rtcpPacket;

    if (!config_.rtcp_cname.empty()) {
        rtcpPacket = rtcpContext_->CreateCompoundPacket(config_.rtcp_cname, config_.rtcp_name);
    } else {
        rtcpPacket = rtcpContext_->CreateRtcpSr();
    }

    if (rtcpPacket && rtcpPacket->Size() > 0) {
        bool success = transportAdapter_->SendRtcpPacket(rtcpPacket->Data(), rtcpPacket->Size());
        if (success) {
            LMRTSP_LOGD("RTCP report sent: size=%zu", rtcpPacket->Size());
        } else {
            LMRTSP_LOGW("Failed to send RTCP report");
        }
    }
}

} // namespace lmshao::lmrtsp