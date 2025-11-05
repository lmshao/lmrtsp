/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_PACKETIZER_H265_H
#define LMSHAO_LMRTSP_RTP_PACKETIZER_H265_H

#include <cstdint>
#include <memory>

#include "i_rtp_packetizer.h"
#include "lmrtsp/media_types.h"

namespace lmshao::lmrtsp {

class RtpPacketizerH265 : public IRtpPacketizer {
public:
    RtpPacketizerH265() = default;
    explicit RtpPacketizerH265(uint32_t ssrc, uint16_t initial_seq = 0, uint8_t payload_type = 98,
                               uint32_t clock_rate = 90000, uint32_t mtu_size = 1400)
        : ssrc_(ssrc), sequenceNumber_(initial_seq), payloadType_(payload_type), clockRate_(clock_rate),
          mtuSize_(mtu_size)
    {
    }

    ~RtpPacketizerH265() override = default;

    void SubmitFrame(const std::shared_ptr<MediaFrame> &frame) override;

private:
    static const uint8_t *FindStartCode(const uint8_t *data, size_t size);
    static const uint8_t *FindNextStartCode(const uint8_t *data, size_t size);

    void PacketizeSingleNalu(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu);
    void PacketizeFuA(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu);

private:
    uint32_t ssrc_ = 0;
    uint16_t sequenceNumber_ = 0;
    uint8_t payloadType_ = 98;   // dynamic for H265
    uint32_t clockRate_ = 90000; // H265 clock
    uint32_t mtuSize_ = 1400;    // default MTU
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_PACKETIZER_H265_H
