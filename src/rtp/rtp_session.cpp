/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtp_session.h"

#include "internal_logger.h"

namespace lmshao::lmrtsp {

RtpSession::RtpSession(std::unique_ptr<IRtpPacketizer> packetizer, std::unique_ptr<ITransport> transport)
    : packetizer_(std::move(packetizer)), transport_(std::move(transport))
{
    LMRTSP_LOGD("RtpSession created");
}

void RtpSession::SendFrame(const MediaFrame &frame)
{
    if (!packetizer_ || !transport_) {
        LMRTSP_LOGE("RtpSession: packetizer or transport is null");
        return;
    }

    LMRTSP_LOGD("RtpSession: sending frame, size: %zu", frame.data.size());
    auto rtp_packets = packetizer_->packetize(frame);
    LMRTSP_LOGD("RtpSession: packetized into %zu RTP packets", rtp_packets.size());

    for (const auto &packet : rtp_packets) {
        transport_->Send(packet.payload.data(), packet.payload.size());
    }
}

} // namespace lmshao::lmrtsp