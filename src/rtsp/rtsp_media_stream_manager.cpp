/**
 * Full implementation of RTSP media stream manager with RTP support.
 */

#include "lmrtsp/rtsp_media_stream_manager.h"

#include <sstream>

#include "../rtp/udp_rtp_transport_adapter.h"
#include "internal_logger.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtp_source_session.h"

namespace lmshao::lmrtsp {

RtspMediaStreamManager::RtspMediaStreamManager(std::weak_ptr<lmshao::lmrtsp::RTSPSession> rtsp_session)
    : rtsp_session_(rtsp_session), state_(StreamState::IDLE), active_(false), send_thread_running_(false),
      sequence_number_(0), timestamp_(0), ssrc_(0)
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
    rtp_session_ = std::make_unique<RtpSourceSession>();

    // Prepare config for RTP session
    RtpSourceSessionConfig rtp_config;
    rtp_config.transport = config;
    rtp_config.video_type = MediaType::H264;
    rtp_config.video_payload_type = 96;
    rtp_config.mtu_size = 1400;
    rtp_config.enable_rtcp = false;

    // Initialize RTP session (this will create and setup transport)
    if (!rtp_session_->Initialize(rtp_config)) {
        LMRTSP_LOGE("Failed to initialize RTP source session");
        rtp_session_.reset();
        return false;
    }

    // Get transport adapter and update config with allocated ports
    if (config.type == TransportConfig::Type::UDP) {
        auto *transport = rtp_session_->GetTransportAdapter();
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
    if (rtp_session_) {
        rtp_session_->Start();
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
    if (rtp_session_) {
        rtp_session_->Stop();
    }

    active_ = false;
    state_ = StreamState::PAUSED;

    LMRTSP_LOGD("Media playback paused");
    return true;
}

void RtspMediaStreamManager::Teardown()
{
    active_ = false;
    send_thread_running_ = false;

    // Stop and cleanup RTP session (it will handle transport cleanup)
    if (rtp_session_) {
        rtp_session_->Stop();
        rtp_session_.reset();
    }

    state_ = StreamState::IDLE;
    LMRTSP_LOGD("Media stream teardown completed");
}

bool RtspMediaStreamManager::PushFrame(const lmrtsp::MediaFrame &frame)
{
    if (!active_ || !rtp_session_) {
        return false;
    }

    // Send frame via RTP
    ProcessFrame(frame);

    timestamp_ = frame.timestamp;
    sequence_number_++;
    return true;
}

void RtspMediaStreamManager::ProcessFrame(const lmrtsp::MediaFrame &frame)
{
    if (!rtp_session_) {
        return;
    }

    // Create a MediaFrame shared pointer for RTP session
    auto frame_ptr = std::make_shared<lmrtsp::MediaFrame>(frame);
    rtp_session_->SendFrame(frame_ptr);
}

std::string RtspMediaStreamManager::GetRtpInfo() const
{
    std::ostringstream oss;
    oss << "seq=" << sequence_number_ << ";rtptime=" << timestamp_;
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
