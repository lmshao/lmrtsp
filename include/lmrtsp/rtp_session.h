/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_SESSION_H
#define LMSHAO_LMRTSP_RTP_SESSION_H

#include <memory>

#include "lmrtsp/i_rtp_packetizer.h"
#include "lmrtsp/i_transport.h"

namespace lmshao::lmrtsp {

class RtpSession {
public:
    RtpSession(std::unique_ptr<IRtpPacketizer> packetizer, std::unique_ptr<ITransport> transport);
    ~RtpSession() = default;

    void SendFrame(const MediaFrame &frame);

private:
    std::unique_ptr<IRtpPacketizer> packetizer_;
    std::unique_ptr<ITransport> transport_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_SESSION_H