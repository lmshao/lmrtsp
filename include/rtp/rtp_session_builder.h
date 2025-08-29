/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTP_RTP_SESSION_BUILDER_H
#define LMSHAO_RTP_RTP_SESSION_BUILDER_H

#include <string>

#include "rtp/rtp_session.h"

namespace lmshao::rtp {

class RtpSessionBuilder {
public:
    RtpSessionBuilder();
    ~RtpSessionBuilder() = default;

    RtpSessionBuilder &WithSsrc(uint32_t ssrc);
    RtpSessionBuilder &WithPayloadType(uint8_t payload_type);
    RtpSessionBuilder &WithClockRate(uint32_t clock_rate);
    RtpSessionBuilder &WithMtuSize(uint32_t mtu_size);
    RtpSessionBuilder &WithRemoteIp(const std::string &remote_ip);
    RtpSessionBuilder &WithRemotePort(uint16_t remote_port);

    std::unique_ptr<RtpSession> Build();

private:
    uint32_t ssrc_ = 0;
    uint8_t payload_type_ = 96;
    uint32_t clock_rate_ = 90000;
    uint32_t mtu_size_ = 1400;
    std::string remote_ip_;
    uint16_t remote_port_ = 0;
};

} // namespace lmshao::rtp

#endif // LMSHAO_RTP_RTP_SESSION_BUILDER_H