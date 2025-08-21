#ifndef RTSP_AAC_PACKETIZER_H
#define RTSP_AAC_PACKETIZER_H

#include "rtsp/rtp/i_rtp_packetizer.h"

namespace lmshao::rtsp::rtp {

class AacPacketizer : public IRtpPacketizer {
public:
    AacPacketizer(uint32_t ssrc, uint16_t sequence_number, uint32_t timestamp, uint32_t mtu_size);
    virtual ~AacPacketizer() = default;

    std::vector<RtpPacket> packetize(const MediaFrame& frame) override;

private:
    uint32_t ssrc_;
    uint16_t sequence_number_;
    uint32_t timestamp_;
    uint32_t mtu_size_;
};

}

#endif // RTSP_AAC_PACKETIZER_H