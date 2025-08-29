/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp/rtp_session.h"

namespace lmshao::rtp {

RtpSession::RtpSession(std::unique_ptr<IRtpPacketizer> packetizer, std::unique_ptr<ITransport> transport)
    : packetizer_(std::move(packetizer)), transport_(std::move(transport))
{
}

void RtpSession::SendFrame(const MediaFrame &frame)
{
    if (!packetizer_ || !transport_) {
        return;
    }

    auto rtp_packets = packetizer_->packetize(frame);
    for (const auto &packet : rtp_packets) {
        transport_->Send(packet.payload.data(), packet.payload.size());
    }
}

} // namespace lmshao::rtp