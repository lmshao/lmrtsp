/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_DEPACKETIZER_TS_H
#define LMSHAO_LMRTSP_RTP_DEPACKETIZER_TS_H

#include <cstdint>
#include <memory>
#include <vector>

#include "i_rtp_depacketizer.h"
#include "lmrtsp/rtp_packet.h"

namespace lmshao::lmrtsp {

/**
 * @brief RTP Depacketizer for MPEG-2 Transport Stream (TS)
 *
 * RFC 2250: RTP Payload Format for MPEG1/MPEG2 Video
 * Extracts TS packets from RTP payload
 */
class RtpDepacketizerTs : public IRtpDepacketizer {
public:
    RtpDepacketizerTs() = default;
    ~RtpDepacketizerTs() override = default;

    void SubmitPacket(const std::shared_ptr<RtpPacket> &packet) override;

private:
    static constexpr size_t TS_PACKET_SIZE = 188; // Standard TS packet size
    static constexpr uint8_t TS_SYNC_BYTE = 0x47; // TS sync byte

    void ProcessTsData(const uint8_t *data, size_t size, uint32_t timestamp);
    bool ValidateTsPacket(const uint8_t *packet);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_DEPACKETIZER_TS_H
