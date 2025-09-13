/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTP_UDP_TRANSPORT_H
#define LMSHAO_LMRTP_UDP_TRANSPORT_H

#include <lmnet/udp_client.h>

#include <memory>

#include "lmrtp/i_transport.h"

using namespace lmshao::lmnet;

namespace lmshao::lmrtp {

class UdpTransport : public ITransport {
public:
    UdpTransport();
    virtual ~UdpTransport();

    bool Init(const std::string &ip, uint16_t port) override;
    bool Send(const uint8_t *data, size_t len) override;
    void Close() override;

private:
    std::shared_ptr<UdpClient> udp_client_;
};

} // namespace lmshao::lmrtp

#endif // LMSHAO_LMRTP_UDP_TRANSPORT_H
