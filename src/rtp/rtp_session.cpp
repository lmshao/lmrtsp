/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtp/rtp_session.h"

#include "internal_logger.h"

namespace lmshao::lmrtp {

RtpSession::RtpSession(std::unique_ptr<IRtpPacketizer> packetizer, std::unique_ptr<ITransport> transport)
    : packetizer_(std::move(packetizer)), transport_(std::move(transport))
{
    RTP_LOGD("RtpSession created");
}

void RtpSession::SendFrame(const MediaFrame &frame)
{
    if (!packetizer_ || !transport_) {
        RTP_LOGE("RtpSession: packetizer or transport is null");
        return;
    }

    RTP_LOGD("RtpSession: sending frame, size: %zu", frame.data.size());
    auto rtp_packets = packetizer_->packetize(frame);
    RTP_LOGD("RtpSession: packetized into %zu RTP packets", rtp_packets.size());

    for (const auto &packet : rtp_packets) {
        transport_->Send(packet.payload.data(), packet.payload.size());
    }
}

} // namespace lmshao::lmrtp