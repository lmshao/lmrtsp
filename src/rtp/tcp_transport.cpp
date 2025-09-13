/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtp/tcp_transport.h"

#include "internal_logger.h"

using namespace lmshao::lmnet;
using namespace lmshao::lmcore;

namespace lmshao::lmrtp {

TcpTransport::TcpTransport()
{
    RTP_LOGD("TcpTransport created");
}

TcpTransport::~TcpTransport()
{
    RTP_LOGD("TcpTransport destroyed");
    Close();
}

bool TcpTransport::Init(const std::string &ip, uint16_t port)
{
    RTP_LOGD("TcpTransport initializing: %s:%d", ip.c_str(), port);
    tcp_client_ = TcpClient::Create(ip, port);
    if (!tcp_client_) {
        RTP_LOGE("Failed to create TCP client");
        return false;
    }
    tcp_client_->SetListener(shared_from_this());
    bool result = tcp_client_->Init();
    if (result) {
        RTP_LOGD("TcpTransport initialized successfully");
    } else {
        RTP_LOGE("Failed to initialize TCP client");
    }
    return result;
}

bool TcpTransport::Send(const uint8_t *data, size_t size)
{
    if (!tcp_client_) {
        RTP_LOGE("TcpTransport: TCP client not initialized");
        return false;
    }
    RTP_LOGD("TcpTransport: sending %zu bytes", size);
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

} // namespace lmshao::lmrtp