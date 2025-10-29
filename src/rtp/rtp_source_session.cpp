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
#include "rtp_packetizer_h264.h"
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
    sequence_number_ = GenerateRandomSequenceNumber();
    timestamp_ = 0;

    // Create transport adapter based on config
    if (config_.transport.type == TransportConfig::Type::UDP) {
        transport_adapter_ = std::make_unique<UdpRtpTransportAdapter>();
    } else if (config_.transport.type == TransportConfig::Type::TCP_INTERLEAVED) {
        // For TCP interleaved, we need RTSP session - simplified implementation
        // In real implementation, this would need proper RTSP session reference
        LMRTSP_LOGE("TCP_INTERLEAVED transport type is not supported in this simple implementation ");
        return false; // Not supported in this simple implementation
    } else {
        return false; // Unsupported transport type
    }

    // Create video packetizer
    if (config_.video_type == MediaType::H264) {
        video_packetizer_ =
            std::make_unique<RtpPacketizerH264>(config_.ssrc, sequence_number_, config_.video_payload_type,
                                                90000, // H264 clock rate
                                                config_.mtu_size);
        // Set up listener for video packetizer
        video_listener_ = std::static_pointer_cast<IRtpPacketizerListener>(
            std::make_shared<PacketizerListener>(transport_adapter_.get()));
        video_packetizer_->SetListener(video_listener_);
    }

    // Check if video packetizer was created
    if (!video_packetizer_) {
        transport_adapter_.reset();
        return false;
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

    // Setup transport
    if (!transport_adapter_->Setup(config_.transport)) {
        LMRTSP_LOGE("Failed to setup transport");
        return false;
    }

    running_ = true;
    return true;
}

void RtpSourceSession::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;

    // Clean up packetizers
    video_packetizer_.reset();

    // Clean up transport
    if (transport_adapter_) {
        transport_adapter_->Close();
        transport_adapter_.reset();
    }
}

bool RtpSourceSession::SendFrame(const std::shared_ptr<MediaFrame> &frame)
{
    LMRTSP_LOGI("SendFrame called - running: %s, frame: %s", running_ ? "true" : "false", frame ? "valid" : "null");

    if (!running_ || !frame) {
        LMRTSP_LOGE("SendFrame failed - running: %s, frame: %s", running_ ? "true" : "false", frame ? "valid" : "null");
        return false;
    }

    // Only support H264 video frames
    if (frame->media_type != MediaType::H264 || !video_packetizer_) {
        LMRTSP_LOGE("SendFrame failed - media_type: %d, video_packetizer: %s", static_cast<int>(frame->media_type),
                    video_packetizer_ ? "valid" : "null");
        return false; // Unsupported media type or no video packetizer
    }

    LMRTSP_LOGI("About to submit frame to packetizer - frame size: %u", frame->data ? frame->data->Size() : 0);

    // Submit frame for packetization
    // The listener is already set up during initialization
    try {
        video_packetizer_->SubmitFrame(frame);
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

// Helper class to handle packetized RTP packets
class RtpSourceSession::PacketizerListener : public IRtpPacketizerListener {
public:
    explicit PacketizerListener(IRtpTransportAdapter *transport) : transport_(transport) {}

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
};

} // namespace lmshao::lmrtsp