/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_PACKETIZER_AAC_H
#define LMSHAO_LMRTSP_RTP_PACKETIZER_AAC_H

#include <cstdint>
#include <memory>

#include "i_rtp_packetizer.h"
#include "lmrtsp/media_types.h"

namespace lmshao::lmrtsp {

class RtpPacketizerAac : public IRtpPacketizer {
public:
    RtpPacketizerAac() = default;
    explicit RtpPacketizerAac(uint32_t ssrc, uint16_t initial_seq = 0, uint8_t payload_type = 97,
                              uint32_t clock_rate = 48000, uint32_t mtu_size = 1400)
        : ssrc_(ssrc), sequenceNumber_(initial_seq), payloadType_(payload_type), clockRate_(clock_rate),
          mtuSize_(mtu_size)
    {
    }

    ~RtpPacketizerAac() override = default;

    void SubmitFrame(const std::shared_ptr<MediaFrame> &frame) override;

private:
    uint32_t ssrc_ = 0;
    uint16_t sequenceNumber_ = 0;
    uint8_t payloadType_ = 97;   // dynamic for AAC
    uint32_t clockRate_ = 48000; // AAC clock (example)
    uint32_t mtuSize_ = 1400;    // default MTU
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_PACKETIZER_AAC_H