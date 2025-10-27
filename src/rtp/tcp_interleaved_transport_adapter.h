/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_TCP_INTERLEAVED_TRANSPORT_ADAPTER_H
#define LMSHAO_LMRTSP_TCP_INTERLEAVED_TRANSPORT_ADAPTER_H

#include <memory>
#include <string>

#include "i_rtp_transport_adapter.h"

namespace lmshao::lmrtsp {

class RTSPSession;

class TcpInterleavedTransportAdapter : public IRtpTransportAdapter {
public:
    explicit TcpInterleavedTransportAdapter(std::weak_ptr<RTSPSession> session);
    ~TcpInterleavedTransportAdapter() override;

    bool Setup(const TransportConfig &config) override;
    bool SendPacket(const uint8_t *data, size_t size) override;
    bool SendRtcpPacket(const uint8_t *data, size_t size) override;
    void Close() override;
    std::string GetTransportInfo() const override;
    bool IsActive() const override;

private:
    bool ValidateChannels(uint8_t rtpChannel, uint8_t rtcpChannel) const;

    std::weak_ptr<RTSPSession> rtspSession_;
    uint8_t rtpChannel_{0};
    uint8_t rtcpChannel_{1};
    bool isSetup_{false};
    std::string transportInfo_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_TCP_INTERLEAVED_TRANSPORT_ADAPTER_H