/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_I_RTP_DEPACKETIZER_H
#define LMSHAO_LMRTSP_I_RTP_DEPACKETIZER_H

#include <memory>
#include <string>

#include "lmrtsp/media_types.h"
#include "lmrtsp/rtp_packet.h"

namespace lmshao::lmrtsp {

class IRtpDepacketizerListener {
public:
    virtual ~IRtpDepacketizerListener() = default;
    virtual void OnFrame(const std::shared_ptr<MediaFrame> &frame) = 0;
    virtual void OnError(int code, const std::string &message) = 0;
};

class IRtpDepacketizer {
public:
    virtual ~IRtpDepacketizer() = default;
    virtual void SubmitPacket(const std::shared_ptr<RtpPacket> &packet) = 0;
    virtual void SetListener(std::shared_ptr<IRtpDepacketizerListener> listener) { listener_ = listener; }

protected:
    std::weak_ptr<IRtpDepacketizerListener> listener_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_I_RTP_DEPACKETIZER_H