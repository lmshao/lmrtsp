/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTP_RTP_SESSION_H
#define LMSHAO_RTP_RTP_SESSION_H

#include <memory>

#include "rtp/i_rtp_packetizer.h"
#include "rtp/i_transport.h"

namespace lmshao::rtp {

class RtpSession {
public:
    RtpSession(std::unique_ptr<IRtpPacketizer> packetizer, std::unique_ptr<ITransport> transport);
    ~RtpSession() = default;

    void SendFrame(const MediaFrame &frame);

private:
    std::unique_ptr<IRtpPacketizer> packetizer_;
    std::unique_ptr<ITransport> transport_;
};

} // namespace lmshao::rtp

#endif // LMSHAO_RTP_RTP_SESSION_H