/**
 * Dummy minimal implementation for build-only mode.
 */

#include "lmrtsp/rtsp_media_stream_manager.h"

#include <sstream>

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
    state_ = StreamState::SETUP;
    return true;
}

bool RtspMediaStreamManager::Play()
{
    if (state_ != StreamState::SETUP && state_ != StreamState::PAUSED) {
        return false;
    }
    active_ = true;
    state_ = StreamState::PLAYING;
    return true;
}

bool RtspMediaStreamManager::Pause()
{
    if (state_ != StreamState::PLAYING) {
        return false;
    }
    active_ = false;
    state_ = StreamState::PAUSED;
    return true;
}

void RtspMediaStreamManager::Teardown()
{
    active_ = false;
    send_thread_running_ = false;
    state_ = StreamState::IDLE;
}

bool RtspMediaStreamManager::PushFrame(const lmrtsp::MediaFrame &frame)
{
    if (!active_) {
        return false;
    }
    timestamp_ = frame.timestamp;
    sequence_number_++;
    return true;
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

void RtspMediaStreamManager::SendMediaThread() {}

void RtspMediaStreamManager::ProcessFrame(const lmrtsp::MediaFrame &) {}

std::unique_ptr<lmshao::lmrtsp::IRtpTransportAdapter>
RtspMediaStreamManager::CreateTransportAdapter(const lmshao::lmrtsp::TransportConfig &)
{
    return nullptr;
}

} // namespace lmshao::lmrtsp
