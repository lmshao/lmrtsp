/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTP_I_RTP_PACKETIZER_H
#define LMSHAO_LMRTP_I_RTP_PACKETIZER_H

#include <memory>
#include <vector>

#include "rtp_packet.h"

namespace lmshao::lmrtp {
// Input frame structure for the packetizer.
struct MediaFrame {
    std::vector<uint8_t> data;
    uint32_t timestamp;
    bool marker = false;
};

// Interface for RTP packetizers.
// Each implementation will handle a specific codec (e.g., H264, AAC).
class IRtpPacketizer {
public:
    virtual ~IRtpPacketizer() = default;

    // Packetize a media frame into one or more RTP packets.
    virtual std::vector<RtpPacket> packetize(const MediaFrame &frame) = 0;
};

} // namespace lmshao::lmrtp

#endif // LMSHAO_LMRTP_I_RTP_PACKETIZER_H