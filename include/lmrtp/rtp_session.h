/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTP_RTP_SESSION_H
#define LMSHAO_LMRTP_RTP_SESSION_H

#include <memory>

#include "lmrtp/i_rtp_packetizer.h"
#include "lmrtp/i_transport.h"

namespace lmshao::lmrtp {

class RtpSession {
public:
    RtpSession(std::unique_ptr<IRtpPacketizer> packetizer, std::unique_ptr<ITransport> transport);
    ~RtpSession() = default;

    void SendFrame(const MediaFrame &frame);

private:
    std::unique_ptr<IRtpPacketizer> packetizer_;
    std::unique_ptr<ITransport> transport_;
};

} // namespace lmshao::lmrtp

#endif // LMSHAO_LMRTP_RTP_SESSION_H