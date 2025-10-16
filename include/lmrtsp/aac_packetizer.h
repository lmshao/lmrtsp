/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_AAC_PACKETIZER_H
#define LMSHAO_LMRTSP_AAC_PACKETIZER_H

#include "lmrtsp/i_rtp_packetizer.h"

namespace lmshao::lmrtsp {

class AacPacketizer : public IRtpPacketizer {
public:
    AacPacketizer(uint32_t ssrc, uint16_t sequence_number, uint32_t timestamp, uint32_t mtu_size);
    virtual ~AacPacketizer() = default;

    std::vector<RtpPacket> packetize(const MediaFrame &frame) override;

private:
    uint32_t ssrc_;
    uint16_t sequence_number_;
    uint32_t timestamp_;
    uint32_t mtu_size_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_AAC_PACKETIZER_H