/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_DEPACKETIZER_H264_H
#define LMSHAO_LMRTSP_RTP_DEPACKETIZER_H264_H

#include <cstdint>
#include <memory>
#include <vector>

#include "i_rtp_depacketizer.h"
#include "lmrtsp/rtp_packet.h"

namespace lmshao::lmrtsp {

class RtpDepacketizerH264 : public IRtpDepacketizer {
public:
    RtpDepacketizerH264() = default;
    ~RtpDepacketizerH264() override = default;

    void SubmitPacket(const std::shared_ptr<RtpPacket> &packet) override;

private:
    void FlushFrame();
    void ResetState(); // Reset depacketizer state on error

    std::vector<uint8_t> pending_;
    uint32_t currentTimestamp_ = 0;
    uint16_t lastSequenceNumber_ = 0; // Track sequence numbers
    bool sequenceInitialized_ = false;
    bool haveFrameData_ = false;
    bool fuaActive_ = false;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_DEPACKETIZER_H264_H