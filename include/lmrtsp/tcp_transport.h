/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_TCP_TRANSPORT_H
#define LMSHAO_LMRTSP_TCP_TRANSPORT_H

#include <lmnet/iclient_listener.h>
#include <lmnet/tcp_client.h>

#include <memory>

#include "lmrtsp/i_transport.h"

using namespace lmshao::lmnet;
using namespace lmshao::lmcore;

namespace lmshao::lmrtsp {

class TcpTransport : public ITransport, public IClientListener, public std::enable_shared_from_this<TcpTransport> {
public:
    TcpTransport();
    virtual ~TcpTransport();

    bool Init(const std::string &ip, uint16_t port) override;
    bool Send(const uint8_t *data, size_t size) override;
    void Close() override;

protected:
    void OnReceive(socket_t fd, std::shared_ptr<DataBuffer> buffer) override;
    void OnClose(socket_t fd) override;
    void OnError(socket_t fd, const std::string &errorInfo) override;

private:
    std::shared_ptr<TcpClient> tcp_client_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_TCP_TRANSPORT_H