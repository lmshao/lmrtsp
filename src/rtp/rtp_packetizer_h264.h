/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_PACKETIZER_H264_H
#define LMSHAO_LMRTSP_RTP_PACKETIZER_H264_H

#include <cstdint>
#include <memory>

#include "i_rtp_packetizer.h"
#include "lmrtsp/media_types.h"

namespace lmshao::lmrtsp {

class RtpPacketizerH264 : public IRtpPacketizer {
public:
    RtpPacketizerH264() = default;
    explicit RtpPacketizerH264(uint32_t ssrc, uint16_t initial_seq = 0, uint8_t payload_type = 96,
                               uint32_t clock_rate = 90000, uint32_t mtu_size = 1400)
        : ssrc_(ssrc), sequence_number_(initial_seq), payload_type_(payload_type), clock_rate_(clock_rate),
          mtu_size_(mtu_size)
    {
    }

    ~RtpPacketizerH264() override = default;

    void SubmitFrame(const std::shared_ptr<MediaFrame> &frame) override;

private:
    // Minimal NALU parsing helpers
    static const uint8_t *FindStartCode(const uint8_t *data, size_t size);
    static const uint8_t *FindNextStartCode(const uint8_t *data, size_t size);

    void PacketizeSingleNalu(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu);
    void PacketizeFuA(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu);

private:
    uint32_t ssrc_ = 0;
    uint16_t sequence_number_ = 0;
    uint8_t payload_type_ = 96;   // dynamic for H264
    uint32_t clock_rate_ = 90000; // H264 clock
    uint32_t mtu_size_ = 1400;    // default MTU
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_PACKETIZER_H264_H