/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_DEPACKETIZER_H265_H
#define LMSHAO_LMRTSP_RTP_DEPACKETIZER_H265_H

#include <cstdint>
#include <memory>
#include <vector>

#include "i_rtp_depacketizer.h"
#include "lmrtsp/rtp_packet.h"

namespace lmshao::lmrtsp {

class RtpDepacketizerH265 : public IRtpDepacketizer {
public:
    RtpDepacketizerH265() = default;
    ~RtpDepacketizerH265() override = default;

    void SubmitPacket(const std::shared_ptr<RtpPacket> &packet) override;

private:
    void FlushFrame();
    void ResetState();

    std::vector<uint8_t> pending_;
    uint32_t currentTimestamp_ = 0;
    uint16_t lastSequenceNumber_ = 0;
    bool sequenceInitialized_ = false;
    bool haveFrameData_ = false;
    bool fuActive_ = false; // H.265 uses FU instead of FU-A
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_DEPACKETIZER_H265_H
