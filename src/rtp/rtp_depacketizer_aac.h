/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_DEPACKETIZER_AAC_H
#define LMSHAO_LMRTSP_RTP_DEPACKETIZER_AAC_H

#include <cstdint>
#include <memory>
#include <vector>

#include "i_rtp_depacketizer.h"
#include "lmrtsp/rtp_packet.h"

namespace lmshao::lmrtsp {

class RtpDepacketizerAac : public IRtpDepacketizer {
public:
    RtpDepacketizerAac() = default;
    ~RtpDepacketizerAac() override = default;

    void SubmitPacket(const std::shared_ptr<RtpPacket> &packet) override;

private:
    void FlushFrame();
    std::vector<uint8_t> pending_;
    uint32_t currentTimestamp_ = 0;
    bool haveFrameData_ = false;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_DEPACKETIZER_AAC_H