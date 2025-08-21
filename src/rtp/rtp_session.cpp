#include "rtsp/rtp/rtp_session.h"

namespace lmshao::rtsp::rtp {

RtpSession::RtpSession(std::unique_ptr<IRtpPacketizer> packetizer, std::unique_ptr<ITransport> transport)
    : packetizer_(std::move(packetizer)), transport_(std::move(transport)) {}

void RtpSession::SendFrame(const MediaFrame& frame) {
    if (!packetizer_ || !transport_) {
        return;
    }

    auto rtp_packets = packetizer_->packetize(frame);
    for (const auto& packet : rtp_packets) {
        transport_->Send(packet.payload.data(), packet.payload.size());
    }
}

}