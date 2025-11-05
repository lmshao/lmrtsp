/**
 * Full implementation of RTSP media stream manager with RTP support.
 */

#include "lmrtsp/rtsp_media_stream_manager.h"

#include <sstream>

#include "internal_logger.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtp_source_session.h"
#include "lmrtsp/rtsp_server_session.h"
#include "rtp/udp_rtp_transport_adapter.h"

namespace lmshao::lmrtsp {

RtspMediaStreamManager::RtspMediaStreamManager(std::weak_ptr<lmshao::lmrtsp::RtspServerSession> rtsp_session)
    : RtspServerSession_(rtsp_session), state_(StreamState::IDLE), active_(false), sendThreadRunning_(false),
      sequenceNumber_(0), timestamp_(0), ssrc_(0)
{
}

RtspMediaStreamManager::~RtspMediaStreamManager()
{
    Teardown();
}

bool RtspMediaStreamManager::Setup(const lmshao::lmrtsp::TransportConfig &config)
{
    // Persist provided transport config for later response headers
    transport_config_ = config;

    // Create RTP source session
    rtpSession_ = std::make_unique<RtpSourceSession>();

    // Get media stream info from RTSP session to determine codec type
    MediaType video_type = MediaType::H264; // default
    uint8_t payload_type = 96;              // default for H264

    LMRTSP_LOGD("RtspMediaStreamManager::Setup - Checking codec type");

    auto session = RtspServerSession_.lock();
    if (session) {
        LMRTSP_LOGI("Successfully locked RtspServerSession");
        auto stream_info = session->GetMediaStreamInfo();
        LMRTSP_LOGI("GetMediaStreamInfo returned: %p", (void *)stream_info.get());
        if (stream_info) {
            LMRTSP_LOGI("Got MediaStreamInfo - codec: %s, payload_type: %d", stream_info->codec.c_str(),
                        stream_info->payload_type);
            // Determine media type from codec string
            if (stream_info->codec == "MP2T") {
                video_type = MediaType::MP2T;
                payload_type = 33; // RFC 3551 payload type for MP2T
                LMRTSP_LOGI("Using MP2T codec with payload type 33");
            } else if (stream_info->codec == "H264") {
                video_type = MediaType::H264;
                payload_type = 96; // Dynamic payload type for H264
                LMRTSP_LOGI("Using H264 codec with payload type 96");
            } else if (stream_info->codec == "H265") {
                video_type = MediaType::H265;
                payload_type = 98; // Dynamic payload type for H265
                LMRTSP_LOGI("Using H265 codec with payload type 98");
            } else if (stream_info->codec == "AAC") {
                video_type = MediaType::AAC;
                payload_type = 97; // Dynamic payload type for AAC
                LMRTSP_LOGI("Using AAC codec with payload type 97");
            }
            // Use payload_type from stream_info if specified
            if (stream_info->payload_type > 0) {
                payload_type = stream_info->payload_type;
            }
        } else {
            LMRTSP_LOGW("No MediaStreamInfo available, using default H264");
        }
    } else {
        LMRTSP_LOGW("Cannot lock RtspServerSession, using default H264");
    }

    LMRTSP_LOGI("Final codec configuration - video_type: %d, payload_type: %d", static_cast<int>(video_type),
                payload_type);

    // Prepare config for RTP session
    RtpSourceSessionConfig rtp_config;
    rtp_config.transport = config;
    rtp_config.video_type = video_type;
    rtp_config.video_payload_type = payload_type;
    rtp_config.mtu_size = 1400;
    rtp_config.enable_rtcp = true;
    // Pass RTSP session for TCP interleaved mode
    rtp_config.rtsp_session = RtspServerSession_;

    // Initialize RTP session (this will create and setup transport)
    if (!rtpSession_->Initialize(rtp_config)) {
        LMRTSP_LOGE("Failed to initialize RTP source session");
        rtpSession_.reset();
        return false;
    }

    // Get transport adapter and update config with allocated ports
    if (config.type == TransportConfig::Type::UDP) {
        auto *transport = rtpSession_->GetTransportAdapter();
        if (transport) {
            auto *udp_adapter = dynamic_cast<UdpRtpTransportAdapter *>(transport);
            if (udp_adapter) {
                transport_config_.server_rtp_port = udp_adapter->GetServerRtpPort();
                transport_config_.server_rtcp_port = udp_adapter->GetServerRtcpPort();
                LMRTSP_LOGD("Allocated UDP ports: server_rtp=%u, server_rtcp=%u, client_rtp=%u, client_rtcp=%u",
                            transport_config_.server_rtp_port, transport_config_.server_rtcp_port,
                            transport_config_.client_rtp_port, transport_config_.client_rtcp_port);
            }
        }
    } else if (config.type == TransportConfig::Type::TCP_INTERLEAVED) {
        LMRTSP_LOGI("TCP interleaved mode: interleaved=%d-%d", config.rtpChannel, config.rtcpChannel);
    }

    state_ = StreamState::SETUP;
    return true;
}

bool RtspMediaStreamManager::Play()
{
    if (state_ != StreamState::SETUP && state_ != StreamState::PAUSED) {
        return false;
    }

    // Start RTP session
    if (rtpSession_) {
        rtpSession_->Start();
    }

    active_ = true;
    state_ = StreamState::PLAYING;

    LMRTSP_LOGD("Media playback started");
    return true;
}

bool RtspMediaStreamManager::Pause()
{
    if (state_ != StreamState::PLAYING) {
        return false;
    }

    // Stop RTP session
    if (rtpSession_) {
        rtpSession_->Stop();
    }

    active_ = false;
    state_ = StreamState::PAUSED;

    LMRTSP_LOGD("Media playback paused");
    return true;
}

void RtspMediaStreamManager::Teardown()
{
    active_ = false;
    sendThreadRunning_ = false;

    // Stop and cleanup RTP session (it will handle transport cleanup)
    if (rtpSession_) {
        rtpSession_->Stop();
        rtpSession_.reset();
    }

    state_ = StreamState::IDLE;
    LMRTSP_LOGD("Media stream teardown completed");
}

bool RtspMediaStreamManager::PushFrame(const lmrtsp::MediaFrame &frame)
{
    if (!active_ || !rtpSession_) {
        return false;
    }

    // Send frame via RTP
    ProcessFrame(frame);

    timestamp_ = frame.timestamp;
    sequenceNumber_++;
    return true;
}

void RtspMediaStreamManager::ProcessFrame(const lmrtsp::MediaFrame &frame)
{
    if (!rtpSession_) {
        return;
    }

    // Create a MediaFrame shared pointer for RTP session
    auto frame_ptr = std::make_shared<lmrtsp::MediaFrame>(frame);
    rtpSession_->SendFrame(frame_ptr);
}

std::string RtspMediaStreamManager::GetRtpInfo() const
{
    std::ostringstream oss;
    oss << "seq=" << sequenceNumber_ << ";rtptime=" << timestamp_;
    return oss.str();
}

std::string RtspMediaStreamManager::GetTransportInfo() const
{
    // Build a valid Transport header based on the persisted config
    std::ostringstream oss;
    if (transport_config_.type == lmshao::lmrtsp::TransportConfig::Type::TCP_INTERLEAVED) {
        oss << "RTP/AVP/TCP";
        if (transport_config_.unicast) {
            oss << ";unicast";
        }
        // RTSP over TCP interleaved channels
        oss << ";interleaved=" << static_cast<int>(transport_config_.rtpChannel) << "-"
            << static_cast<int>(transport_config_.rtcpChannel);
    } else {
        // UDP transport with client/server ports
        oss << "RTP/AVP";
        if (transport_config_.unicast) {
            oss << ";unicast";
        }
        if (transport_config_.client_rtp_port || transport_config_.client_rtcp_port) {
            oss << ";client_port=" << transport_config_.client_rtp_port << "-" << transport_config_.client_rtcp_port;
        }
        if (transport_config_.server_rtp_port || transport_config_.server_rtcp_port) {
            oss << ";server_port=" << transport_config_.server_rtp_port << "-" << transport_config_.server_rtcp_port;
        }
    }
    return oss.str();
}

bool RtspMediaStreamManager::IsActive() const
{
    return active_;
}

void RtspMediaStreamManager::SendMediaThread()
{
    // This method can be used for threaded media sending if needed
    // Currently using direct PushFrame approach from main thread
}

} // namespace lmshao::lmrtsp
