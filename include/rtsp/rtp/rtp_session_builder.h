#ifndef RTSP_RTP_SESSION_BUILDER_H
#define RTSP_RTP_SESSION_BUILDER_H

#include "rtsp/rtp/rtp_session.h"
#include <string>

namespace lmshao::rtsp::rtp {

class RtpSessionBuilder {
public:
    RtpSessionBuilder();
    ~RtpSessionBuilder() = default;

    RtpSessionBuilder& WithSsrc(uint32_t ssrc);
    RtpSessionBuilder& WithPayloadType(uint8_t payload_type);
    RtpSessionBuilder& WithClockRate(uint32_t clock_rate);
    RtpSessionBuilder& WithMtuSize(uint32_t mtu_size);
    RtpSessionBuilder& WithRemoteIp(const std::string& remote_ip);
    RtpSessionBuilder& WithRemotePort(uint16_t remote_port);

    std::unique_ptr<RtpSession> Build();

private:
    uint32_t ssrc_ = 0;
    uint8_t payload_type_ = 96;
    uint32_t clock_rate_ = 90000;
    uint32_t mtu_size_ = 1400;
    std::string remote_ip_;
    uint16_t remote_port_ = 0;
};

}

#endif // RTSP_RTP_SESSION_BUILDER_H