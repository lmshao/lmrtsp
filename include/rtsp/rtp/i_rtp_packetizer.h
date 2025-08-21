#ifndef I_RTP_PACKETIZER_H
#define I_RTP_PACKETIZER_H

#include "rtp_packet.h"
#include <vector>
#include <memory>

namespace lmshao::rtsp::rtp {
    // Input frame structure for the packetizer.
    struct MediaFrame
    {
        std::vector<uint8_t> data;
        uint32_t timestamp;
        bool marker = false;
    };

    // Interface for RTP packetizers.
    // Each implementation will handle a specific codec (e.g., H264, AAC).
    class IRtpPacketizer
    {
    public:
        virtual ~IRtpPacketizer() = default;

        // Packetize a media frame into one or more RTP packets.
        virtual std::vector<RtpPacket> packetize(const MediaFrame& frame) = 0;
    };

} // namespace rtsp

#endif // I_RTP_PACKETIZER_H