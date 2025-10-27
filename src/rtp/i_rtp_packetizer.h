/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_I_RTP_PACKETIZER_H
#define LMSHAO_LMRTSP_I_RTP_PACKETIZER_H

#include <memory>
#include <string>

#include "lmrtsp/media_types.h"
#include "lmrtsp/rtp_packet.h"

namespace lmshao::lmrtsp {

class IRtpPacketizerListener {
public:
    virtual ~IRtpPacketizerListener() = default;
    virtual void OnPacket(const std::shared_ptr<RtpPacket> &packet) = 0;
    virtual void OnError(int code, const std::string &message) = 0;
};

class IRtpPacketizer {
public:
    virtual ~IRtpPacketizer() = default;
    virtual void SubmitFrame(const std::shared_ptr<MediaFrame> &frame) = 0;
    virtual void SetListener(std::shared_ptr<IRtpPacketizerListener> listener) { listener_ = listener; }

protected:
    std::weak_ptr<IRtpPacketizerListener> listener_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_I_RTP_PACKETIZER_H