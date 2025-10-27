/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_I_RTP_TRANSPORT_ADAPTER_H
#define LMSHAO_LMRTSP_I_RTP_TRANSPORT_ADAPTER_H

#include <cstdint>
#include <string>

#include "lmrtsp/transport_config.h"

namespace lmshao::lmrtsp {

class IRtpTransportAdapter {
public:
    virtual ~IRtpTransportAdapter() = default;

    virtual bool Setup(const TransportConfig &config) = 0;
    virtual bool SendPacket(const uint8_t *data, size_t size) = 0;
    virtual bool SendRtcpPacket(const uint8_t *data, size_t size) = 0;
    virtual void Close() = 0;
    virtual std::string GetTransportInfo() const = 0;
    virtual bool IsActive() const = 0;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_I_RTP_TRANSPORT_ADAPTER_H