/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/udp_transport.h"

#include <lmnet/udp_client.h>

#include "internal_logger.h"

using namespace lmshao::lmnet;

namespace lmshao::lmrtsp {

UdpTransport::UdpTransport() : udp_client_(nullptr)
{
    LMRTSP_LOGD("UdpTransport created");
}

UdpTransport::~UdpTransport()
{
    LMRTSP_LOGD("UdpTransport destroyed");
    Close();
}

bool UdpTransport::Init(const std::string &ip, uint16_t port)
{
    LMRTSP_LOGD("UdpTransport initializing: %s:%d", ip.c_str(), port);
    udp_client_ = UdpClient::Create(ip, port);
    if (!udp_client_) {
        LMRTSP_LOGE("Failed to create UDP client");
        return false;
    }
    bool result = udp_client_->Init();
    if (result) {
        LMRTSP_LOGD("UdpTransport initialized successfully");
    } else {
        LMRTSP_LOGE("Failed to initialize UDP client");
    }
    return result;
}

bool UdpTransport::Send(const uint8_t *data, size_t len)
{
    if (!udp_client_) {
        LMRTSP_LOGE("UdpTransport: UDP client not initialized");
        return false;
    }
    LMRTSP_LOGD("UdpTransport: sending %zu bytes", len);
    return udp_client_->Send(data, len);
}

void UdpTransport::Close()
{
    if (udp_client_) {
        udp_client_->Close();
        udp_client_.reset();
    }
}

} // namespace lmshao::lmrtsp