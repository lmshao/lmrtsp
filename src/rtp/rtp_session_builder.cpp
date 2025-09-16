/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtp/rtp_session_builder.h"

#include "internal_logger.h"
#include "lmrtp/h264_packetizer.h"
#include "lmrtp/udp_transport.h"

namespace lmshao::lmrtp {

RtpSessionBuilder::RtpSessionBuilder()
{
    RTP_LOGD("RtpSessionBuilder created");
}

RtpSessionBuilder &RtpSessionBuilder::WithSsrc(uint32_t ssrc)
{
    ssrc_ = ssrc;
    return *this;
}

RtpSessionBuilder &RtpSessionBuilder::WithPayloadType(uint8_t payload_type)
{
    payload_type_ = payload_type;
    return *this;
}

RtpSessionBuilder &RtpSessionBuilder::WithClockRate(uint32_t clock_rate)
{
    clock_rate_ = clock_rate;
    return *this;
}

RtpSessionBuilder &RtpSessionBuilder::WithMtuSize(uint32_t mtu_size)
{
    mtu_size_ = mtu_size;
    return *this;
}

RtpSessionBuilder &RtpSessionBuilder::WithRemoteIp(const std::string &remote_ip)
{
    remote_ip_ = remote_ip;
    return *this;
}

RtpSessionBuilder &RtpSessionBuilder::WithRemotePort(uint16_t remote_port)
{
    remote_port_ = remote_port;
    return *this;
}

std::unique_ptr<RtpSession> RtpSessionBuilder::Build()
{
    RTP_LOGD("RtpSessionBuilder: building RTP session with SSRC=0x%08X, remote=%s:%d", ssrc_, remote_ip_.c_str(),
             remote_port_);
    auto packetizer = std::make_unique<H264Packetizer>(ssrc_, 0, 0, mtu_size_);
    auto transport = std::make_unique<UdpTransport>();
    if (!transport->Init(remote_ip_, remote_port_)) {
        RTP_LOGE("RtpSessionBuilder: failed to initialize transport");
        return nullptr;
    }
    RTP_LOGD("RtpSessionBuilder: RTP session built successfully");
    return std::make_unique<RtpSession>(std::move(packetizer), std::move(transport));
}

} // namespace lmshao::lmrtp