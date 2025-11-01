/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_PACKETIZER_TS_H
#define LMSHAO_LMRTSP_RTP_PACKETIZER_TS_H

#include <cstdint>
#include <memory>
#include <vector>

#include "i_rtp_packetizer.h"
#include "lmrtsp/rtp_packet.h"

namespace lmshao::lmrtsp {

/**
 * @brief RTP Packetizer for MPEG-2 Transport Stream (TS)
 *
 * RFC 2250: RTP Payload Format for MPEG1/MPEG2 Video
 * For MPEG-2 TS, multiple TS packets (188 bytes each) are packed into RTP payload
 */
class RtpPacketizerTs : public IRtpPacketizer {
public:
    RtpPacketizerTs() = default;
    ~RtpPacketizerTs() override = default;

    void SubmitFrame(const std::shared_ptr<MediaFrame> &frame) override;

    void SetSsrc(uint32_t ssrc) { ssrc_ = ssrc; }
    void SetPayloadType(uint8_t pt) { payloadType_ = pt; }
    void SetMtuSize(uint32_t mtu) { mtuSize_ = mtu; }

private:
    static constexpr size_t TS_PACKET_SIZE = 188; // Standard TS packet size

    void PacketizeTs(const uint8_t *data, size_t size, uint32_t timestamp);

    uint32_t ssrc_ = 0;
    uint16_t sequenceNumber_ = 0;
    uint8_t payloadType_ = 33;   // Static PT for MP2T (RFC 3551)
    uint32_t clockRate_ = 90000; // 90kHz clock for MPEG-2 TS
    uint32_t mtuSize_ = 1400;    // Default MTU
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_PACKETIZER_TS_H
