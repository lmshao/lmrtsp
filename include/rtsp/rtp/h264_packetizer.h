#ifndef RTSP_H264_PACKETIZER_H
#define RTSP_H264_PACKETIZER_H

#include "rtsp/rtp/i_rtp_packetizer.h"

namespace lmshao::rtsp::rtp {

class H264Packetizer : public IRtpPacketizer {
public:
    H264Packetizer(uint32_t ssrc, uint16_t sequence_number, uint32_t timestamp, uint32_t mtu_size);
    virtual ~H264Packetizer() = default;

    std::vector<RtpPacket> packetize(const MediaFrame& frame) override;

private:
    void PacketizeSingleNalu(const uint8_t* nalu, size_t nalu_size);
    void PacketizeFuA(const uint8_t* nalu, size_t nalu_size);

    uint32_t ssrc_;
    uint16_t sequence_number_;
    uint32_t timestamp_;
    uint32_t mtu_size_;
    std::vector<RtpPacket> packets_;
};

}

#endif // RTSP_H264_PACKETIZER_H
