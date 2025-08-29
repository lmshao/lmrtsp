/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp/udp_transport.h"

#include <network/udp_client.h>

using namespace lmshao::network;

namespace lmshao::rtp {

UdpTransport::UdpTransport() : udp_client_(nullptr) {}

UdpTransport::~UdpTransport()
{
    Close();
}

bool UdpTransport::Init(const std::string &ip, uint16_t port)
{
    udp_client_ = UdpClient::Create(ip, port);
    if (!udp_client_) {
        return false;
    }
    return udp_client_->Init();
}

bool UdpTransport::Send(const uint8_t *data, size_t len)
{
    if (!udp_client_) {
        return false;
    }
    return udp_client_->Send(data, len);
}

void UdpTransport::Close()
{
    if (udp_client_) {
        udp_client_->Close();
        udp_client_.reset();
    }
}

} // namespace lmshao::rtp