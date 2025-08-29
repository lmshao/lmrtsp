/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp/tcp_transport.h"

using namespace lmshao::network;
using namespace lmshao::coreutils;

namespace lmshao::rtp {

TcpTransport::TcpTransport() = default;

TcpTransport::~TcpTransport()
{
    Close();
}

bool TcpTransport::Init(const std::string &ip, uint16_t port)
{
    tcp_client_ = TcpClient::Create(ip, port);
    if (!tcp_client_) {
        return false;
    }
    tcp_client_->SetListener(shared_from_this());
    return tcp_client_->Init();
}

bool TcpTransport::Send(const uint8_t *data, size_t size)
{
    if (!tcp_client_) {
        return false;
    }
    return tcp_client_->Send(reinterpret_cast<const char *>(data), size);
}

void TcpTransport::Close()
{
    if (tcp_client_) {
        tcp_client_->Close();
    }
}

void TcpTransport::OnReceive(socket_t fd, std::shared_ptr<DataBuffer> buffer)
{
    // Not used for sending RTP data
}

void TcpTransport::OnClose(socket_t fd)
{
    // Not used for sending RTP data
}

void TcpTransport::OnError(socket_t fd, const std::string &errorInfo)
{
    // Not used for sending RTP data
}

} // namespace lmshao::rtp