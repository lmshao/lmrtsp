#include "rtsp/rtp/rtp_session_builder.h"
#include "rtsp/rtp/h264_packetizer.h"
#include "rtsp/rtp/udp_transport.h"

namespace lmshao::rtsp::rtp {

RtpSessionBuilder::RtpSessionBuilder() = default;

RtpSessionBuilder& RtpSessionBuilder::WithSsrc(uint32_t ssrc) {
    ssrc_ = ssrc;
    return *this;
}

RtpSessionBuilder& RtpSessionBuilder::WithPayloadType(uint8_t payload_type) {
    payload_type_ = payload_type;
    return *this;
}

RtpSessionBuilder& RtpSessionBuilder::WithClockRate(uint32_t clock_rate) {
    clock_rate_ = clock_rate;
    return *this;
}

RtpSessionBuilder& RtpSessionBuilder::WithMtuSize(uint32_t mtu_size) {
    mtu_size_ = mtu_size;
    return *this;
}

RtpSessionBuilder& RtpSessionBuilder::WithRemoteIp(const std::string& remote_ip) {
    remote_ip_ = remote_ip;
    return *this;
}

RtpSessionBuilder& RtpSessionBuilder::WithRemotePort(uint16_t remote_port) {
    remote_port_ = remote_port;
    return *this;
}

std::unique_ptr<RtpSession> RtpSessionBuilder::Build() {
    auto packetizer = std::make_unique<lmshao::rtsp::rtp::H264Packetizer>(ssrc_, 0, 0, mtu_size_);
    auto transport = std::make_unique<lmshao::rtsp::rtp::UdpTransport>();
    if (!transport->Init(remote_ip_, remote_port_)) {
        return nullptr;
    }
    return std::make_unique<lmshao::rtsp::rtp::RtpSession>(std::move(packetizer), std::move(transport));
}

}